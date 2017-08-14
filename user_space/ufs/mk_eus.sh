#!/bin/sh

rm -rf ./tmp
mkdir tmp
chmod 755 tmp

echo "Overlaying images"
cp -Rv ./initramfs/* ./tmp/
chmod -Rv 777 ../liblitmus/out/
cp -Rv ../liblitmus/out/ ./tmp/bin
cp -Rv ../feather-trace-tools/out/ ./tmp/bin
cp -Rv ../ss_parser/ss_parser ./tmp/bin

echo "Creating cpio image"
cd ./tmp
find ./ | cpio -H newc -o > ../initramfs.cpio
echo "Creating Final initramfs.igz"
cd ..
cat initramfs.cpio | gzip > initramfs.igz
#rm initramfs.cpio
#rm -rf ./tmp

