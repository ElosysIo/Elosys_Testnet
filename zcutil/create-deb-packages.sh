export APP_VERSION="5.8.1"

echo -n "Building amd64 deb..........."
debdir=bin/elosys-qt-ubuntu1804-v$APP_VERSION
mkdir -p $debdir > /dev/null
mkdir    $debdir/DEBIAN
mkdir -p $debdir/usr/local/bin

cat zcutil/deb/control_amd64 | sed "s/RELEASE_VERSION/$APP_VERSION/g" > $debdir/DEBIAN/control

cp release/elosys-qt-linux                   $debdir/usr/local/bin/elosys-qt

mkdir -p $debdir/usr/share/pixmaps/
cp zcutil/deb/elosys.xpm           $debdir/usr/share/pixmaps/

mkdir -p $debdir/usr/share/applications
cp zcutil/deb/desktopentry    $debdir/usr/share/applications/elosys-qt.desktop

dpkg-deb --build $debdir >/dev/null
cp $debdir.deb                 release/elosys-qt-ubuntu1804-v$APP_VERSION.deb
rm ./bin -rf
echo "[OK]"


echo -n "Building aarch64 deb..........."
debdir=bin/elosys-qt-arrch64-v$APP_VERSION
mkdir -p $debdir > /dev/null
mkdir    $debdir/DEBIAN
mkdir -p $debdir/usr/local/bin

cat zcutil/deb/control_aarch64 | sed "s/RELEASE_VERSION/$APP_VERSION/g" > $debdir/DEBIAN/control

cp release/elosys-qt-arm                   $debdir/usr/local/bin/elosys-qt

mkdir -p $debdir/usr/share/pixmaps/
cp zcutil/deb/elosys.xpm           $debdir/usr/share/pixmaps/

mkdir -p $debdir/usr/share/applications
cp zcutil/deb/desktopentry    $debdir/usr/share/applications/elosyswallet-qt.desktop

dpkg-deb --build $debdir >/dev/null
cp $debdir.deb                 release/elosys-qt-aarch64-v$APP_VERSION.deb
rm ./bin -rf
echo "[OK]"
