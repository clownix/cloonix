#!/bin/bash
# make_win_zip.sh - the client, the demos and the installer, in one zip to
# carry to a windows machine.
#   ./make_win_zip.sh                 writes /tmp/cloonix-client-win-<v>.zip
#   ./make_win_zip.sh <dir>           writes it there instead
# There is nothing to build: the client is python source and so are the
# demos. The zip exists only so that one file crosses to the windows machine
# instead of a directory tree, and a zip is what windows explorer opens with
# no tool installed.
# The whole source tree also works: win_client/install.bat run from a copy of
# the tree finds cloonix/ and demos/ one level up. The zip is for the case
# where the tree is not what you want to carry.
set -e
HERE=$(cd $(dirname $0); pwd)
TOP=$(cd ${HERE}/..; pwd)
OUT=${1:-/tmp}

CLIENT="${TOP}/cloonix"
DEMOS="${TOP}/demos"
CFG="${CLIENT}/cloonix_config.py"

for i in "${CLIENT}/cloonix" "${CFG}" "${DEMOS}" \
         "${HERE}/install.bat" "${HERE}/install.ps1" \
         "${HERE}/uninstall.ps1" "${HERE}/README.txt"; do
  if [ ! -e "${i}" ]; then
    echo "${i} missing"
    exit 1
  fi
done

VERSION=$(grep -o 'CLOONIX_VERSION=[^ ]*' "${CFG}" | head -1)
VERSION=${VERSION#*=}
NAME="cloonix-client-win-${VERSION}"
STAGE="/tmp/${NAME}"

rm -rf "${STAGE}" "${OUT}/${NAME}.zip"
mkdir -p "${STAGE}/bin"

# The client and every cloonix_*.py it imports, cloonix_config.py among
# them: the nets, their ports and their passwords travel with the client.
cp "${CLIENT}/cloonix" "${STAGE}/bin/"
cp "${CLIENT}"/cloonix_*.py "${STAGE}/bin/"

# The demos as they are. A demo drives the server across the network, so a
# windows machine plays one exactly as a linux one does.
cp -r "${DEMOS}" "${STAGE}/demos"
find "${STAGE}/demos" -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true

cp "${HERE}/install.bat" "${HERE}/install.ps1" \
   "${HERE}/uninstall.ps1" "${HERE}/README.txt" "${STAGE}/"

cd /tmp
python3 -m zipfile -c "${OUT}/${NAME}.zip" "${NAME}"
rm -rf "${STAGE}"

echo "${OUT}/${NAME}.zip"
echo
echo "Copy it to the windows machine, extract it, and run install.bat"
echo "inside the extracted directory."
