#!/bin/bash
#
# Copyright (c) 2020 The Orbit Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fail on any error.
set -e

if [ -z "${CONAN_PROFILE}" ]; then
  CONAN_PROFILE="$(basename $(dirname "${KOKORO_JOB_NAME}"))"

  # That's a temporary work-around. As long as the kokoro jobs don't carry the
  # conan profile in its name this mapping will do the trick.
  declare -A profile_mapping=( \
    [gcp_ubuntu]="clang7_relwithdebinfo" \
    [gcp_ubuntu_ggp]="ggp_relwithdebinfo" \
    [gcp_ubuntu_test]="clang7_relwithdebinfo" \
    [gcp_windows]="msvc2017_relwithdebinfo" \
    [gcp_windows_test]="msvc2017_relwithdebinfo" \
  )

  CONAN_PROFILE="${profile_mapping[${CONAN_PROFILE}]-${CONAN_PROFILE}}"
fi

if [ -z "${CONAN_PROFILE}" ]; then
  echo -n "No conan profile was set nor one could be auto-detected. "
  echo "Aborting this build!"
  exit 2
fi

source $(cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd)/upload_symbols.sh

if [ -n "$1" ]; then
  # We are inside the docker container
  echo "build.sh is invoked from inside the docker container."
  
  readonly REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../" >/dev/null 2>&1 && pwd )"
  readonly MOUNT_POINT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../../" >/dev/null 2>&1 && pwd )"
  readonly KEYSTORE_PATH="${MOUNT_POINT}/keystore"

  export QT_QPA_PLATFORM=offscreen

  if [ -z "$BUILD_TYPE" ]; then
    readonly BUILD_TYPE="$(basename "$KOKORO_JOB_NAME")"
  fi

  function cleanup {
    # Delete all unnecessary files from the src/-directory.
    # Kokoro would copy them otherwise before applying the artifacts regex
    echo "Delete all unnecessary files from the src/-directory."
    
    set +e # This is allowed to fail when deleting
    if [ "${BUILD_TYPE}" == "presubmit" ]; then
      echo "Cleanup for presubmit."
      find "$MOUNT_POINT" -depth -mindepth 1 | \
        grep -v 'orbitprofiler/kokoro' | \
        grep -v 'testresults/' | \
        while read file; do
          if [[ -d $file ]]; then
            # That might give an error message when the directory is not empty.
            # That's okay.
			echo "Delete directory: $file."
            rmdir --ignore-fail-on-non-empty "$file"
          elif [[ -e $file ]]; then
			echo "Delete file: $file."
            rm "$file"
          fi
        done
      echo "Cleanup for presubmit done."
    else
      # In the non-presubmit case we spare the whole build dir and this script.
      echo "Cleanup for non-presubmit."
      find "$MOUNT_POINT" -depth -mindepth 1 | \
        grep -v 'orbitprofiler/kokoro' | \
        grep -v 'orbitprofiler/build' | \
        while read file; do
          if [[ -d $file ]]; then
            # That might give an error message when the directory is not empty.
            # That's okay.
            rmdir --ignore-fail-on-non-empty "$file"
          elif [[ -e $file ]]; then
            rm "$file"
          fi
        done
      echo "Cleanup for non-presubmit done."
   fi
  }
  trap cleanup EXIT

  echo "Using conan profile ${CONAN_PROFILE} and performing a ${BUILD_TYPE} build."

  set +e
  if [ "$BUILD_TYPE" = "release" ] \
     && [ -z "${ORBIT_BYPASS_RELEASE_CHECK}" ] \
     && ! git -C "${REPO_ROOT}" describe --tags --exact-match > /dev/null; then
    echo -n "We are currently conducting a release build, but we aren't on a tag."
    echo    " Aborting the build..."
    echo -n "Maybe you missed pushing the release version tag?"
    echo    " Please consult the release playbook for advice."
    exit 1
  fi
  set -e

  # That's a temporary solution. The docker containers should have the
  # correct version of conan already preinstalled. This step will be removed
  # when the docker containers are restructured and versioned.
  pip3 install conan==1.29.2

  echo "Installing conan configuration (profiles, settings, etc.)..."
  ${REPO_ROOT}/third_party/conan/configs/install.sh


  if [ "$(uname -s)" == "Linux" ]; then
    readonly OS="linux"
  else
    readonly OS="windows"
  fi

  CRASHDUMP_SERVER=""
  readonly CRASH_SERVER_URL_FILE="${KEYSTORE_PATH}/74938_orbitprofiler_crashdump_collection_server"
  if [ -f "$CRASH_SERVER_URL_FILE" ]; then
    CRASHDUMP_SERVER="$(cat "${CRASH_SERVER_URL_FILE}" | tr -d '\n')"
  fi

  # Building Orbit
  mkdir -p "${REPO_ROOT}/build/"

  if [[ $CONAN_PROFILE == ggp_* ]]; then
    readonly PACKAGING_OPTION="-o debian_packaging=True"
  else
    readonly PACKAGING_OPTION=""
  fi

  echo "Invoking conan lock."
  conan lock create "${REPO_ROOT}/conanfile.py" --user=orbitdeps --channel=stable \
    --build=outdated \
    --lockfile="${REPO_ROOT}/third_party/conan/lockfiles/base.lock" -u -pr ${CONAN_PROFILE} \
    -o crashdump_server="$CRASHDUMP_SERVER" $PACKAGING_OPTION \
    --lockfile-out="${REPO_ROOT}/build/conan.lock"

  echo "Installs the requirements (conan install)."
  conan install -if "${REPO_ROOT}/build/" \
          --build outdated \
          --lockfile="${REPO_ROOT}/build/conan.lock" \
          "${REPO_ROOT}" | sed 's/^crashdump_server=.*$/crashump_server=<<hidden>>/'
  echo "Starting the build (conan build)."
  conan build -bf "${REPO_ROOT}/build/" "${REPO_ROOT}"
  echo "Start the packaging (conan package)."
  conan package -bf "${REPO_ROOT}/build/" "${REPO_ROOT}"
  echo "Packaging is done."

  # Uploading symbols to the symbol server
  if [ "${BUILD_TYPE}" == "release" ] \
     || [ "${BUILD_TYPE}" == "nightly" ] \
     || [ "${BUILD_TYPE}" == "continuous_on_release_branch" ]; then
    set +e
    echo "Uploading symbols to the symbol server."
    api_key=$(get_api_key "${OAUTH_TOKEN_HEADER}")
    upload_debug_symbols "${api_key}" "${REPO_ROOT}/build/bin"
    set -e
  fi

  # Signing the debian package
  if [ -f "${KEYSTORE_PATH}/74938_SigningPrivateGpg" ] && [[ $CONAN_PROFILE == ggp_* ]]; then
    echo "Signing the debian package"
    rm -rf ~/.gnupg/
    rm -rf /dev/shm/signing.gpg
    mkdir -p ~/.gnupg
    chmod 700 ~/.gnupg
    echo "allow-loopback-pinentry" > ~/.gnupg/gpg-agent.conf

    GPG_OPTIONS="--pinentry-mode loopback --batch --no-tty --yes --no-default-keyring --keyring /dev/shm/signing.gpg --passphrase-file ${KEYSTORE_PATH}/74938_SigningPrivateGpgKeyPassword"

    gpg ${GPG_OPTIONS} --import ${KEYSTORE_PATH}/74938_SigningPrivateGpg

    for deb in ${REPO_ROOT}/build/package/*.deb; do
      gpg ${GPG_OPTIONS} --output "$deb.asc" --detach-sign "$deb"
    done
  fi

  # Package the Debian package, the signature and the ggp client into a zip for integration in the 
  # installer.
  if [ -f ${KEYSTORE_PATH}/74938_SigningPrivateGpg ] && [[ $CONAN_PROFILE == ggp_* ]]; then
    echo "Create a zip containing OrbitService for integration in the installer."
    pushd "${REPO_ROOT}/build/package" > /dev/null
    mkdir -p Orbit/collector
    cp -v OrbitProfiler*.deb Orbit/collector/
    cp -v OrbitProfiler*.deb.asc Orbit/collector/
    cp -v bin/OrbitClientGgp Orbit/collector/
    zip Collector.zip -r Orbit/
    rm -rf Orbit/
    popd > /dev/null
  fi

  # Package build artifacts into a zip for integration in the installer.
  if [[ $CONAN_PROFILE != ggp_* ]]; then
    echo "Create a zip containing Orbit UI for integration in the installer."
    pushd "${REPO_ROOT}/build/package" > /dev/null
    cp -av bin/ Orbit
    find Orbit/ -name \*.pdb -delete
    cp -v NOTICE Orbit/NOTICE
    test -f NOTICE.Chromium && cp -v NOTICE.Chromium Orbit/NOTICE.Chromium
    cp -v LICENSE Orbit/LICENSE.txt
    cp -av "${REPO_ROOT}/contrib/automation_tests" Orbit
    cp -v "${REPO_ROOT}/Orbit.h" Orbit/
    zip -r Orbit.zip Orbit/
    rm -rf Orbit/
    popd > /dev/null
  fi

  exit $?
fi

# --------------------------------------
# This part only executes when NOT in docker:

# We can't access the Keys-API inside of a docker container. So we retrieve
# the key before entering the containers and transport it via environment variable.
echo "build.sh is invoked on the VM - bring up docker."
TEMP_DIR="$(mktemp -d)"
pushd "${TEMP_DIR}" > /dev/null
install_oauth2l
export OAUTH_TOKEN_HEADER=$(retrieve_oauth_token_header)
remove_oauth2l
popd > /dev/null
rm -rf "${TEMP_DIR}"

readonly CONTAINER="gcr.io/orbitprofiler/${CONAN_PROFILE}:latest"

if [ "$(uname -s)" == "Linux" ]; then
  echo "Bring up docker for Linux build."
  gcloud auth configure-docker --quiet
  docker pull "${CONTAINER}"
  docker run --rm -v ${KOKORO_ARTIFACTS_DIR}:/mnt \
    -e KOKORO_JOB_NAME -e CONAN_PROFILE -e BUILD_TYPE \
    -e OAUTH_TOKEN_HEADER -e ORBIT_BYPASS_RELEASE_CHECK \
    ${CONTAINER} \
    /mnt/github/orbitprofiler/kokoro/builds/build.sh in_docker
else
  echo "Bring up docker for Windows build."
  gcloud.cmd auth configure-docker --quiet
  docker pull "${CONTAINER}"
  docker run --rm -v ${KOKORO_ARTIFACTS_DIR}:C:/mnt \
    -e KOKORO_JOB_NAME -e CONAN_PROFILE -e BUILD_TYPE \
    -e OAUTH_TOKEN_HEADER -e ORBIT_BYPASS_RELEASE_CHECK \
    --isolation=process --storage-opt 'size=50GB' \
    ${CONTAINER} \
    'C:/Program Files/Git/bin/bash.exe' -c \
    "/c/mnt/github/orbitprofiler/kokoro/builds/build.sh in_docker"
fi
