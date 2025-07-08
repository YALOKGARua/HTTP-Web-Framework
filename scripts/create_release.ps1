param(
    [Parameter(Mandatory=$true)]
    [string]$Version,
    
    [Parameter(Mandatory=$false)]
    [string]$Message = "Release $Version"
)

if (-not $Version.StartsWith("v")) {
    $Version = "v$Version"
}

Write-Host "Creating release $Version..." -ForegroundColor Green

try {
    $currentBranch = git branch --show-current
    if ($currentBranch -ne "main") {
        Write-Warning "Not on main branch. Current branch: $currentBranch"
        $continue = Read-Host "Continue? (y/N)"
        if ($continue -ne "y" -and $continue -ne "Y") {
            exit 1
        }
    }

    Write-Host "Checking working tree..." -ForegroundColor Yellow
    $status = git status --porcelain
    if ($status) {
        Write-Error "Working tree is not clean. Please commit or stash changes."
        exit 1
    }

    Write-Host "Pulling latest changes..." -ForegroundColor Yellow
    git pull origin main

    Write-Host "Updating VERSION file..." -ForegroundColor Yellow
    $versionNumber = $Version.TrimStart('v')
    Set-Content -Path "VERSION" -Value $versionNumber

    Write-Host "Committing version update..." -ForegroundColor Yellow
    git add VERSION
    git commit -m "chore: bump version to $Version"

    Write-Host "Creating and pushing tag..." -ForegroundColor Yellow
    git tag -a $Version -m $Message
    git push origin main
    git push origin $Version

    Write-Host "Release $Version created successfully!" -ForegroundColor Green
    Write-Host "GitHub Actions will automatically build and publish the release." -ForegroundColor Cyan
    Write-Host "Check: https://github.com/YALOKGARua/HTTP-Web-Framework/actions" -ForegroundColor Cyan

} catch {
    Write-Error "Failed to create release: $_"
    exit 1
} 