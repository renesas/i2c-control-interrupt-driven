#Requires -Version 5.1
<#
.SYNOPSIS
    Full workflow: uploads a CMake ZIP project and links it to an evaluation kit via GraphQL.

.PARAMETER DryRun
    When set, skips all mutations (no data changes). Queries still run.

.PARAMETER AllowCreateSoftwareProject
  When false, a missing software project fails before any upload or mutation. Default: true.

.PARAMETER Environment
    Target environment: Dev | UAT | Prod.

.PARAMETER Token
  Bearer token for authentication.

.PARAMETER SoftwareProjectName
    Name of the software project (e.g. "Bit Banged UART").

.PARAMETER EvalKitName
    Name of the evaluation kit (e.g. "FPB-RA2E3").

.PARAMETER ZipFilePath
    Full path to the CMake ZIP file to upload.

.PARAMETER PublisherId
    Publisher ID (default: 572 for Renesas).

.PARAMETER ProjectType
    Project type (default: EXAMPLE).

.PARAMETER ArtifactVersion
    Artifact version string (default: fsp_6.5.0).

.PARAMETER Family
    MCU family (default: RA).

.PARAMETER Framework
    Framework name (default: FSP).

.PARAMETER CompatibleEvalKitId
  Optional fallback compatibility relationship ID when the verification query returns no ID.
  Normally resolved from compatibleEvalKitId in the verification result; distinct from EvalKitId.

.PARAMETER ResultRecord
    Optional shared result record for batch callers. Reports Result and Status even if the script throws.

.EXAMPLE
Run a dry run. Queries still run, but mutations are skipped and no data is changed.
  .\upload_cmake.ps1 -DryRun -Environment Dev -Token "jwt-token" -SoftwareProjectName "Bit Banged UART" -EvalKitName "FPB-RA2E3" -ZipFilePath "C:\Data\BitBangedUART_FPB_RA2E3.zip"

.EXAMPLE
Run normally and allow data changes.
  .\upload_cmake.ps1 -Environment Dev -Token "jwt-token" -SoftwareProjectName "Bit Banged UART" -EvalKitName "FPB-RA2E3" -ZipFilePath "C:\Data\BitBangedUART_FPB_RA2E3.zip"
#>
param(
    [switch]$DryRun,

    [ValidateSet("Dev", "UAT", "Prod")]
    [string]$Environment = "Dev",

    [Parameter(Mandatory = $true)]
    [string]$Token,

    [Parameter(Mandatory = $true)]
    [string]$SoftwareProjectName,

    [Parameter(Mandatory = $true)]
    [string]$EvalKitName,

    [Parameter(Mandatory = $true)]
    [string]$ZipFilePath,

    [string]$PublisherId      = "572",
    [string]$ProjectType      = "EXAMPLE",
    [string]$ArtifactVersion  = "fsp_6.5.0",
    [string]$Family           = "RA",
    [string]$Framework        = "FSP",
    [string]$ToolchainId,
    [string]$ToolchainVersion,
    [string]$CompatibleEvalKitId,
    [hashtable]$ResultRecord = @{},
    [bool]$AllowCreateSoftwareProject = $true
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
  $ResultRecord.Result = "Failed"
  $ResultRecord.Status = "Unknown"
  $ResultRecord.CmakeUrl = ""

# ── Endpoint resolution ──────────────────────────────────────────────────────

$EndpointMap = @{  
  Dev  = @{ GraphQL = "https://api.nexardev.com/graphql/"; Portal = "https://dev-portal-api.data-platform.octopart.net/graphql/" ; Internal_GraphQL = "https://dev.eval-kit.solution-templates.octopart.net/internal/graphql/" }
  UAT  = @{ GraphQL = "https://api.nexaruat.com/graphql/"; Portal = "https://uat-portal-api.data-platform.octopart.net/graphql/" ; Internal_GraphQL = "https://uat.eval-kit.solution-templates.octopart.net/internal/graphql/" }
  Prod = @{ GraphQL = "https://api.nexar.com/graphql/"; Portal = "https://prod-portal-api.data-platform.octopart.net/graphql/"; Internal_GraphQL = "https://prod.eval-kit.solution-templates.octopart.net/internal/graphql/" }
}

$GraphQLEndpoint = $EndpointMap[$Environment].GraphQL
$PortalEndpoint  = $EndpointMap[$Environment].Portal
$InternalEndpoint = $EndpointMap[$Environment].Internal_GraphQL

Write-Host "Environment : $Environment" -ForegroundColor White
Write-Host "GraphQL     : $GraphQLEndpoint" -ForegroundColor White
Write-Host "Portal API  : $PortalEndpoint" -ForegroundColor White
Write-Host "Internal    : $InternalEndpoint" -ForegroundColor White
if ($DryRun) {
    Write-Host "[DRY RUN] Mutations will be skipped — no data changes will be made." -ForegroundColor Yellow
}
Write-Host ""

# ── Helpers ──────────────────────────────────────────────────────────────────

function Invoke-GraphQL {
    param(
        [string]$OperationName,
        [string]$Query,
        [hashtable]$Variables  = @{},
        [string]$Endpoint      = $GraphQLEndpoint
    )

    $isMutationOp = $Query -match '^\s*mutation\b'

    if ($DryRun -and $isMutationOp) {
        Write-Host "[DRY RUN] $OperationName — mutation request (not executed):" -ForegroundColor DarkYellow
        Write-Host ($Query.Trim()) -ForegroundColor DarkGray
        Write-Host "Variables: $($Variables | ConvertTo-Json -Depth 10 -Compress)" -ForegroundColor DarkGray
        return $null
    }

    Write-Host "[RUN] $OperationName ..." -ForegroundColor Cyan

    $body = @{ query = $Query; variables = $Variables } | ConvertTo-Json -Depth 10

    $headers = @{
        "Content-Type"  = "application/json"
        "Authorization" = "Bearer $Token"
    }

    try {
        $response = Invoke-RestMethod -Uri $Endpoint -Method POST -Headers $headers -Body $body
        if ($response.PSObject.Properties['errors'] -and $response.errors) {
            Write-Warning "$OperationName returned errors:"
            $response.errors | ForEach-Object { Write-Warning "  $($_.message)" }
        }
          if ($response.PSObject.Properties['data']) {
            return $response.data
          }
          return $null
    }
    catch {
        Write-Error "$OperationName failed: $_"
        return $null
    }
}

# ── 1.1  Search / create software project ────────────────────────────────────

$searchProjectQuery = @'
query searchSoftwareProject(
  $where: SupSoftwareProjectSearchFilterInput
  $first: Int
) {
  supSoftwareProjectSearch(where: $where, first: $first) {
    edges {
      node {
        id
        title
      }
    }
  }
}
'@

$searchResult = Invoke-GraphQL `
    -OperationName "searchSoftwareProject" `
    -Query $searchProjectQuery `
    -Variables @{ first = 10; where = @{ title = $SoftwareProjectName } }

$softwareProjectId = $null
if ($searchResult) {
  $projects = @($searchResult.supSoftwareProjectSearch.edges | Where-Object { $_.node.title -ieq $SoftwareProjectName })
  Write-Host "  Found $($projects.Count) project(s) exactly matching '$SoftwareProjectName'" -ForegroundColor Green
    $projects | ForEach-Object { Write-Host "    id=$($_.node.id)  title=$($_.node.title)" }
    if ($projects.Count -gt 0) {
        $softwareProjectId = $projects[0].node.id
    }
}

if (-not $softwareProjectId) {
  if (-not $AllowCreateSoftwareProject) {
    throw "Software project '$SoftwareProjectName' was not found and creation is disabled (-AllowCreateSoftwareProject `$false)."
  }
    Write-Host "  Project not found — will create it." -ForegroundColor Yellow

    $createProjectMutation = @'
mutation Admin_CreateSoftwareProject($input: SupSoftwareProjectCreateSoftwareProjectInput!) {
  supSoftwareProjectCreateSoftwareProject(input: $input) {
    id
    errors {
      ... on SupSoftwareProjectOperationFailedError {
        errorCode
        message
      }
    }
  }
}
'@

    $createResult = Invoke-GraphQL `
        -OperationName "Admin_CreateSoftwareProject" `
        -Query $createProjectMutation `
        -Variables @{
            input = @{
                title              = $SoftwareProjectName
                description        = ""
                recommendScore     = 0
                publisherId        = $PublisherId
                type               = $ProjectType
                compatibleEvalKits = @()
            }
        }

    if ($createResult) {
        $softwareProjectId = $createResult.supSoftwareProjectCreateSoftwareProject.id
        Write-Host "  Created project id=$softwareProjectId" -ForegroundColor Green
    }
}

Write-Host ""

# ── 1.2  Search for evaluation kit ───────────────────────────────────────────

$searchEvalKitQuery = @'
query searchEvalKit($where: SupEvalKitSearchFilterInput, $first: Int) {
  supEvalKitSearch(where: $where, first: $first) {
    edges {
      node {
        id
        title
        sourceUrl
      }
    }
  }
}
'@

$evalKitResult = Invoke-GraphQL `
    -OperationName "searchEvalKit" `
    -Query $searchEvalKitQuery `
    -Variables @{ first = 10; where = @{ q = $EvalKitName } }

$evalKitId = $null
if ($evalKitResult) {
  $kits = @($evalKitResult.supEvalKitSearch.edges | Where-Object { $_.node.title -ieq $EvalKitName })
  Write-Host "  Found $($kits.Count) eval kit(s) exactly matching '$EvalKitName'" -ForegroundColor Green
    $kits | ForEach-Object { Write-Host "    id=$($_.node.id)  title=$($_.node.title)" }
    if ($kits.Count -gt 0) {
        $evalKitId = $kits[0].node.id
    }
}

Write-Host ""

# ── 1.3  Upload ZIP to File Service (Portal API, multipart) ──────────────────

$fileId       = $null
$fileType     = $null
$fileNameOnly = $null

if ($DryRun) {
    Write-Host "[DRY RUN] UploadFile — mutation request (not executed):" -ForegroundColor DarkYellow
    $uploadQueryPreview = @'
mutation UploadFile($file: Upload!) {
  supRefUploadFileToNexarFileService(input: { file: $file }) {
    result { fileId fileType fileName }
    errors { ... on OperationFailedError { message errorCode } }
  }
}
'@
    Write-Host ($uploadQueryPreview.Trim()) -ForegroundColor DarkGray
    Write-Host "Variables: { file: `"$ZipFilePath`" }" -ForegroundColor DarkGray
}
elseif (-not (Test-Path $ZipFilePath)) {
    Write-Warning "ZIP file not found at '$ZipFilePath' — skipping upload (steps 1.3, 1.4, 1.5)."
}
else {
    Write-Host "[RUN] UploadFile ..." -ForegroundColor Cyan

    $uploadQuery = @'
mutation UploadFile($file: Upload!) {
  supRefUploadFileToNexarFileService(input: { file: $file }) {
    result {
      fileId
      fileType
      fileName
    }
    errors {
      ... on OperationFailedError {
        message
        errorCode
      }
    }
  }
}
'@

    $form = [ordered]@{
        operations = @{
            query     = $uploadQuery
            variables = @{ file = $null }
        } | ConvertTo-Json -Depth 5 -Compress
        map        = '{"0":["variables.file"]}'
        '0'        = Get-Item -LiteralPath $ZipFilePath
    }

    try {
        $uploadResponse = Invoke-RestMethod `
            -Uri $PortalEndpoint `
            -Method Post `
            -Headers @{
                "Authorization"    = "Bearer $Token"
                "GraphQL-preflight" = "1"
            } `
            -Form $form

        if ($uploadResponse.PSObject.Properties['errors'] -and $uploadResponse.errors) {
            Write-Warning "UploadFile returned errors:"
            $uploadResponse.errors | ForEach-Object { Write-Warning "  $($_.message)" }
        }
        else {
            $fileResult   = $uploadResponse.data.supRefUploadFileToNexarFileService.result
            $fileId       = $fileResult.fileId
            $fileType     = $fileResult.fileType
            $fileNameOnly = $fileResult.fileName
            Write-Host "  Uploaded: fileId=$fileId  fileName=$fileNameOnly" -ForegroundColor Green
        }
    }
    catch {
        Write-Error "UploadFile failed: $_"
    }
}

Write-Host ""

# ── 1.4  Attach source ZIP to eval kit ───────────────────────────────────────

if (($softwareProjectId -and $evalKitId -and $fileId) -or $DryRun) {

  $verifySourceQuery = @'
query verifySwCompatEvalKit(
  $softwareProjectId: ID!
  $evalKitIds: [ID!]
) {
  supEvalKitSoftwareProjectCompatibleEvalKitSearch(
  softwareProjectId: $softwareProjectId
  where: { evalKitIds: $evalKitIds }
  ) {
  nodes {
    compatibleEvalKitId    
    projectSources {
    projectSourceId
    configXmlUrl
    artifactVersion
    sourceUrl
    type
    family
    framework
    updatedAt
    }
  }
  }
}
'@

  $existingCompatibility = $null
  if ($softwareProjectId -and $evalKitId) {
    $existingResult = Invoke-GraphQL `
      -OperationName "verifySwCompatEvalKit" `
      -Query $verifySourceQuery `
      -Variables @{
        softwareProjectId = $softwareProjectId
        evalKitIds        = @($evalKitId)
      }

    if (-not $existingResult) {
      throw "Cannot verify existing compatible eval kit sources; attachment was not changed."
    }

    $existingNodes = @($existingResult.supEvalKitSoftwareProjectCompatibleEvalKitSearch.nodes)
    if ($existingNodes.Count -gt 0) {
      $existingCompatibility = $existingNodes[0]
    }
  }

  $sourceFile = @{
    fileId   = $fileId
    fileType = $fileType
    fileName = $fileNameOnly
  }
  $existingCMakeSources = @(
    if ($existingCompatibility) {
      $existingCompatibility.projectSources | Where-Object { $_.type -eq "CMAKE" }
    }
  )

  $patchCompatibleEvalKitMutation = @'
mutation Admin_supEvalKitPatchCompatibleEvalKit($input: SupEvalKitPatchCompatibleEvalKitInput!) {
  supEvalKitPatchCompatibleEvalKit(input: $input) {
    addedCompatibleEvalKits {
      compatibleEvalKitId
      evalKitId
    }
    success
    errors {
      ... on SupEvalKitOperationFailedError {
        message
        errorCode
      }
    }
  }
}
'@

  $patchSourcesMutation = @'
mutation Admin_supEvalKitPatchCompatibleEvalKitProjectSources($input: SupEvalKitPatchCompatibleEvalKitProjectSourcesInput!) {
  supEvalKitPatchCompatibleEvalKitProjectSources(input: $input) {
    taskIds
    success
    errors {
      ... on SupEvalKitOperationFailedError {
        message
        errorCode
      }
      ... on SupEvalKitNotFoundError {
        message
        errorCode
      }
    }
  }
}
'@

  # Resolve the compatibility relationship ID (create it first when missing).
  $resolvedCompatibleEvalKitId = $CompatibleEvalKitId
  if ($existingCompatibility) {
    $returnedCompatibilityId = $existingCompatibility.PSObject.Properties['compatibleEvalKitId']
    if ($returnedCompatibilityId -and -not [string]::IsNullOrWhiteSpace($returnedCompatibilityId.Value)) {
      $resolvedCompatibleEvalKitId = $returnedCompatibilityId.Value
    }
  }

  if ($existingCMakeSources.Count -gt 0) {
    $ResultRecord.Status = "Update"
    if ([string]::IsNullOrWhiteSpace($resolvedCompatibleEvalKitId)) {
      throw "An existing CMAKE source was found, but verification returned no compatibleEvalKitId. Supply -CompatibleEvalKitId as a fallback; attachment was not changed."
    }
    $existingSourceId = $existingCMakeSources[0].PSObject.Properties['projectSourceId']
    if (-not $existingSourceId -or [string]::IsNullOrWhiteSpace($existingSourceId.Value)) {
      throw "An existing CMAKE source was found, but verification returned no project source id; attachment was not changed."
    }
    $updateSource = @{
      projectSourceId    = $existingSourceId.Value
      artifactUpdateMode = "KEEP"
      sourceFile         = $sourceFile
      configXmlUrl       = ""
    }
    if (-not [string]::IsNullOrWhiteSpace($ToolchainId))      { $updateSource.toolchainId      = $ToolchainId }
    if (-not [string]::IsNullOrWhiteSpace($ToolchainVersion)) { $updateSource.toolchainVersion = $ToolchainVersion }

    Write-Host "  CMAKE source found - updating project source." -ForegroundColor Cyan
    $patchResult = Invoke-GraphQL `
      -OperationName "Admin_supEvalKitPatchCompatibleEvalKitProjectSources" `
      -Query $patchSourcesMutation `
      -Endpoint $InternalEndpoint `
      -Variables @{
        input = @{
          compatibleEvalKitId  = $resolvedCompatibleEvalKitId
          updateProjectSources = @($updateSource)
        }
      }
  }
  else {
    $ResultRecord.Status = "Insert"
    Write-Host "  No CMAKE source found - inserting project source." -ForegroundColor Cyan

    if ([string]::IsNullOrWhiteSpace($resolvedCompatibleEvalKitId)) {
      # Step 1: create the software project <-> eval kit compatibility.
      $addResult = Invoke-GraphQL `
        -OperationName "Admin_supEvalKitPatchCompatibleEvalKit" `
        -Query $patchCompatibleEvalKitMutation `
        -Endpoint $InternalEndpoint `
        -Variables @{
          input = @{
            softwareProjectId     = $softwareProjectId
            addCompatibleEvalKits = @($evalKitId)
          }
        }

      if ($DryRun) {
        $resolvedCompatibleEvalKitId = "<compatibleEvalKitId from previous mutation>"
      }
      else {
        if (-not $addResult) {
          throw "Compatible eval kit creation returned no result."
        }
        $addPayload = $addResult.supEvalKitPatchCompatibleEvalKit
        $addErrors = @($addPayload.errors | Where-Object { $null -ne $_ })
        if ($addErrors.Count -gt 0 -or -not $addPayload.success) {
          throw "Compatible eval kit creation failed: $($addErrors | ConvertTo-Json -Depth 5 -Compress)"
        }
        $added = @($addPayload.addedCompatibleEvalKits | Where-Object { $_.evalKitId -eq $evalKitId })
        if ($added.Count -eq 0) { $added = @($addPayload.addedCompatibleEvalKits) }
        if ($added.Count -eq 0 -or [string]::IsNullOrWhiteSpace($added[0].compatibleEvalKitId)) {
          throw "Compatible eval kit creation returned no compatibleEvalKitId."
        }
        $resolvedCompatibleEvalKitId = $added[0].compatibleEvalKitId
      }
    }

    # Step 2: add the CMAKE project source.
    $addSource = @{
      artifactMode    = "NONE"
      type            = "CMAKE"
      family          = $Family
      framework       = $Framework
      artifactVersion = $ArtifactVersion
      sourceFile      = $sourceFile
      configXmlUrl    = ""
    }
    if (-not [string]::IsNullOrWhiteSpace($ToolchainId))      { $addSource.toolchainId      = $ToolchainId }
    if (-not [string]::IsNullOrWhiteSpace($ToolchainVersion)) { $addSource.toolchainVersion = $ToolchainVersion }

    $patchResult = Invoke-GraphQL `
      -OperationName "Admin_supEvalKitPatchCompatibleEvalKitProjectSources" `
      -Query $patchSourcesMutation `
      -Endpoint $InternalEndpoint `
      -Variables @{
        input = @{
          compatibleEvalKitId = $resolvedCompatibleEvalKitId
          addProjectSources   = @($addSource)
        }
      }
  }

  if (-not $DryRun) {
    if (-not $patchResult) {
      throw "Project source patch returned no result."
    }
    $patchPayload = $patchResult.supEvalKitPatchCompatibleEvalKitProjectSources
    $patchErrors = @($patchPayload.errors | Where-Object { $null -ne $_ })
    if ($patchErrors.Count -gt 0 -or -not $patchPayload.success) {
      throw "Project source patch failed: $($patchErrors | ConvertTo-Json -Depth 5 -Compress)"
    }
    $ResultRecord.Result = "Success"
    Write-Host "  Source attached to eval kit." -ForegroundColor Green
  }
  else {
    $ResultRecord.Result = "DryRun"
  }

    Write-Host ""

    # ── 1.5  Verify uploaded source ──────────────────────────────────────────

    if ($softwareProjectId -and $evalKitId) {
      $verifyResult = Invoke-GraphQL `
        -OperationName "verifySwCompatEvalKit" `
        -Query $verifySourceQuery `
        -Variables @{
          softwareProjectId = $softwareProjectId
          evalKitIds        = @($evalKitId)
        }

      if ($verifyResult) {
        $nodes = $verifyResult.supEvalKitSoftwareProjectCompatibleEvalKitSearch.nodes
        $verifiedCMakeSources = @(
          $nodes | ForEach-Object { $_.projectSources } | Where-Object { $_.type -eq "CMAKE" }
        )
        if ($verifiedCMakeSources.Count -gt 0) {
          $ResultRecord.CmakeUrl = [string]$verifiedCMakeSources[0].sourceUrl
        }
        Write-Host "  Verification — CMAKE sources found:" -ForegroundColor Green
        $nodes | ForEach-Object {
          $_.projectSources | Where-Object { $_.type -eq "CMAKE" } | ForEach-Object {
            Write-Host "    sourceUrl=$($_.sourceUrl)  updatedAt=$($_.updatedAt)"
          }
            }
        }
    }
    else {
      Write-Host "[INFO] Skipping step 1.5 - missing softwareProjectId or evalKitId." -ForegroundColor DarkGray
      Write-Host ($verifySourceQuery.Trim()) -ForegroundColor DarkGray
    }

    Write-Host ""
}
elseif (-not $DryRun) {
    Write-Host "[INFO] Skipping steps 1.4 and 1.5 — missing softwareProjectId, evalKitId, or fileId." -ForegroundColor DarkGray
    Write-Host ""
}

# ── 2.1  Link software project back to eval kit ───────────────────────────────

$linkBackMutation = @'
mutation supSoftwareProjectUpdateEvalKitCompatibleSoftwareProject(
  $input: SupSoftwareProjectUpdateEvalKitCompatibleSoftwareProjectInput!
) {
  supSoftwareProjectUpdateEvalKitCompatibleSoftwareProject(input: $input) {
    errors {
      ... on SupSoftwareProjectOperationFailedError {
        message
        errorCode
      }
    }
  }
}
'@

$verifyLinkQuery = @'
query Admin_supSoftwareProjectEvalKitCompatibleSoftwareProjectSearch(
  $evalKitId: ID!
  $where: SupEvalKitCompatibleSoftwareProjectFilterInput
  $first: Int
) {
  supSoftwareProjectEvalKitCompatibleSoftwareProjectSearch(
    evalKitId: $evalKitId
    where: $where
    first: $first
  ) {
    nodes {
      id
      title
    }
  }
}
'@

if ($softwareProjectId -and $evalKitId) {

    $linkResult = Invoke-GraphQL `
        -OperationName "supSoftwareProjectUpdateEvalKitCompatibleSoftwareProject" `
        -Query $linkBackMutation `
        -Variables @{
            input = @{
                id                               = $evalKitId
                addCompatibleSoftwareProjectIds  = @($softwareProjectId)
                removeCompatibleSoftwareProjectIds = @()
            }
        }

    if ($linkResult) {
        $linkErrors = $linkResult.supSoftwareProjectUpdateEvalKitCompatibleSoftwareProject.errors
        if ($linkErrors -and $linkErrors.Count -gt 0) {
            Write-Host "  Link already exists or duplicate — skipping (this is safe)." -ForegroundColor DarkYellow
        }
        else {
            Write-Host "  Software project linked back to eval kit." -ForegroundColor Green
        }
    }

    Write-Host ""

    # ── 2.2  Verify the link ──────────────────────────────────────────────────

    $verifyLinkResult = Invoke-GraphQL `
        -OperationName "Admin_supSoftwareProjectEvalKitCompatibleSoftwareProjectSearch" `
        -Query $verifyLinkQuery `
        -Variables @{
            evalKitId = $evalKitId
            first     = 100
            where     = @{ title = $SoftwareProjectName }
        }

    if ($verifyLinkResult) {
        $linked = $verifyLinkResult.supSoftwareProjectEvalKitCompatibleSoftwareProjectSearch.nodes
        Write-Host "  Verification — evalkitid = $evalKitId — linked software projects:" -ForegroundColor Green
        $linked | ForEach-Object { Write-Host "    id=$($_.id)  title=$($_.title)" }
    }
}
else {
    Write-Host "[INFO] Skipping steps 2.1 and 2.2 — missing softwareProjectId or evalKitId." -ForegroundColor DarkGray
  Write-Host ($linkBackMutation.Trim()) -ForegroundColor DarkGray 
  Write-Host ""
  Write-Host ($verifyLinkQuery.Trim()) -ForegroundColor DarkGray
}
