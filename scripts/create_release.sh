#!/bin/bash

set -e

VERSION=""
MESSAGE=""

usage() {
    echo "Usage: $0 -v VERSION [-m MESSAGE]"
    echo "  -v VERSION    Version to release (e.g., 1.0.0 or v1.0.0)"
    echo "  -m MESSAGE    Release message (optional)"
    exit 1
}

while getopts "v:m:h" opt; do
    case $opt in
        v) VERSION="$OPTARG" ;;
        m) MESSAGE="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

if [ -z "$VERSION" ]; then
    echo "Error: Version is required"
    usage
fi

if [[ ! "$VERSION" =~ ^v ]]; then
    VERSION="v$VERSION"
fi

if [ -z "$MESSAGE" ]; then
    MESSAGE="Release $VERSION"
fi

echo "Creating release $VERSION..."

CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ]; then
    echo "Warning: Not on main branch. Current branch: $CURRENT_BRANCH"
    read -p "Continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "Checking working tree..."
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Working tree is not clean. Please commit or stash changes."
    exit 1
fi

echo "Pulling latest changes..."
git pull origin main

echo "Updating VERSION file..."
VERSION_NUMBER=${VERSION#v}
echo "$VERSION_NUMBER" > VERSION

echo "Committing version update..."
git add VERSION
git commit -m "chore: bump version to $VERSION"

echo "Creating and pushing tag..."
git tag -a "$VERSION" -m "$MESSAGE"
git push origin main
git push origin "$VERSION"

echo "Release $VERSION created successfully!"
echo "GitHub Actions will automatically build and publish the release."
echo "Check: https://github.com/YALOKGARua/HTTP-Web-Framework/actions" 