#!/bin/sh
# Copy the toolchain out of the hmbtn-build image into sdk/ (git-ignored) so the
# host can build without Docker.
#   sdk/psyq-4.7/     PSY-Q 4.7 libraries
#   sdk/include/      PSY-Q 4.7 headers, lower-case (-DPSYQ_INCLUDE_DIR=sdk/include)
#   sdk/gcc-2.95.2/   cpp and cc1 (static 32-bit binaries, run on the host as is)
#   sdk/bin/          mips-linux-gnu-* wrappers that run the image's binutils
set -eu
cd "$(dirname "$0")/.."
image=${1:-hmbtn-build}
c=$(docker create "$image")
trap 'docker rm "$c" >/dev/null' EXIT
rm -rf sdk/psyq-4.7 sdk/include sdk/gcc-2.95.2 sdk/bin
docker cp "$c":/opt/psyq/sdk-4.7 sdk/psyq-4.7
docker cp "$c":/opt/psyq/include sdk/include
docker cp "$c":/opt/psyq/gcc-2.95.2 sdk/gcc-2.95.2
mkdir -p sdk/bin
for t in as ld objcopy nm objdump readelf; do
    cat > sdk/bin/mips-linux-gnu-$t <<WRAP
#!/bin/sh
# Runs the build image's mips-linux-gnu-$t (the MIPS binutils aren't installed on the host).
exec docker run --rm -i --user "\$(id -u):\$(id -g)" -v "\$HOME":"\$HOME" -w "\$PWD" $image mips-linux-gnu-$t "\$@"
WRAP
    chmod +x sdk/bin/mips-linux-gnu-$t
done
echo "installed the SDK and toolchain under sdk/"
