#!/bin/sh
set -u

WD=$(pwd)
APP=kcm-grub2
VERSION="0.4.5"
MODULE=playground
SUBMODULE=sysadmin
THRESHOLD=80

echo "== Start =="
echo
echo "== Fetching ${APP} source... =="
git clone kde:${APP}
rm -rf ${WD}/${APP}/.git
rm ${WD}/${APP}/Messages.sh
rm ${WD}/${APP}/release.sh

echo
echo "== Patching ${APP} CMakeLists.txt... =="
echo "find_package( Msgfmt REQUIRED )" >> ${WD}/${APP}/CMakeLists.txt
echo "find_package( Gettext REQUIRED )" >> ${WD}/${APP}/CMakeLists.txt
echo "add_subdirectory( po )" >> ${WD}/${APP}/CMakeLists.txt

echo
echo "== Fetching translations which are >=${THRESHOLD}% complete... =="
for lang in $(svn cat svn://anonsvn.kde.org/home/kde/trunk/l10n-kde4/subdirs); do
    test "${lang}" == "x-test" && continue
    echo
    echo "-- ${lang} --"

    unset TRANS
    unset FUZZY
    unset UNTRANS
    mkdir -p ${WD}/${APP}/po/${lang}
    cd ${WD}/${APP}/po/${lang}
    svn cat svn://anonsvn.kde.org/home/kde/trunk/l10n-kde4/${lang}/messages/${MODULE}-${SUBMODULE}/${APP}.po >${WD}/${APP}/po/${lang}/${APP}.po 2>/dev/null
    if test "$?" -ne "0"; then
        echo "  No po file found."
        rm -r ${WD}/${APP}/po/${lang}
        continue
    fi
    STAT_STR=$(msgfmt --check --statistics -o /dev/null 2>&1 ${WD}/${APP}/po/${lang}/${APP}.po)
    TRANS=$(echo ${STAT_STR}| awk '{print $1}')
    if test "untranslated" == "$(echo ${STAT_STR}| awk '{print $5}')"; then
        UNTRANS=$(echo ${STAT_STR}| awk '{print $4}')
    else
        if test "fuzzy" == "$(echo ${STAT_STR}| awk '{print $5}')"; then
            FUZZY=$(echo ${STAT_STR}| awk '{print $4}')
        fi
        if test "untranslated" == "$(echo ${STAT_STR}| awk '{print $8}')"; then
            UNTRANS=$(echo ${STAT_STR}| awk '{print $7}')
        fi
    fi
    TRANS=${TRANS:-0}
    FUZZY=${FUZZY:-0}
    UNTRANS=${UNTRANS:-0}

    TOTAL=$[${TRANS}+${FUZZY}+${UNTRANS}]
    PERCENT=$[$[${TRANS}*100]/${TOTAL}]
    if test "${PERCENT}" -ge "${THRESHOLD}"; then
        echo "  Included (${PERCENT}%)."
        echo "file( GLOB _po_files *.po )" >> ${WD}/${APP}/po/${lang}/CMakeLists.txt
        echo "GETTEXT_PROCESS_PO_FILES( ${lang} ALL INSTALL_DESTINATION \${LOCALE_INSTALL_DIR} \${_po_files} )" >> ${WD}/${APP}/po/${lang}/CMakeLists.txt
        echo "add_subdirectory( ${lang} )" >> ${WD}/${APP}/po/CMakeLists.txt
        LANGS+=" ${lang}"
    else
        echo "  Not included (${PERCENT}%)."
        rm -r ${WD}/${APP}/po/${lang}
    fi
done

echo
echo "Languages included in the release:"
echo ${LANGS}

echo
echo "== Renaming ${WD}/${APP} to ${WD}/${APP}-${VERSION}... =="
mv ${WD}/${APP} ${WD}/${APP}-${VERSION}
echo
echo "== Creating tarball ${WD}/${APP}-${VERSION}.tar.gz... =="
cd ${WD}
tar czvf ${APP}-${VERSION}.tar.gz ${APP}-${VERSION}
rm -rf ${APP}-${VERSION}

echo
echo "== Finished =="
