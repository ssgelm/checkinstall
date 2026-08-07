#!/bin/bash
# The backup tree must mirror real directories. A path reached through a
# symlinked directory recorded as-is turns that symlink into a directory
# when the backup is restored over / (#1107638).
. "$HARNESS"

t=$WORK/tree; mkdir -p "$t/usr/lib"; ln -s usr/lib "$t/lib"
echo original > "$t/usr/lib/existing"

cat > "$WORK/write.sh" <<INNER
#!/bin/sh
echo modified > $t/lib/existing
echo new > $t/lib/created
INNER
chmod +x "$WORK/write.sh"

run_iw "$WORK/root-bk" 0 "$WORK/write.sh" >/dev/null 2>&1
b=$WORK/root-bk/BACKUP

if [ -d "$b$t/lib" ] && [ ! -L "$b$t/lib" ]; then
	fail "the backup tree holds a real directory where the system has a symlink"
else
	ok "the backup tree has no directory standing in for the symlink"
fi
if [ -f "$b$t/usr/lib/existing" ] && grep -q original "$b$t/usr/lib/existing"; then
	ok "the overwritten file was saved under its real path"
else
	fail "the overwritten file was not saved under its real path"
fi
if [ -e "$b$t/usr/lib/created" ]; then
	fail "a file the install created was backed up, though it did not exist before"
else
	ok "a newly created file was not backed up"
fi

# Restoring has to leave the symlink alone whatever the tree holds. The
# backup mirrors absolute paths, so the fake root needs the same layout.
r=$WORK/restore; mkdir -p "$r$(dirname "$t")"; cp -a "$t" "$r$(dirname "$t")/"
rm -rf "$b/no-backup"
tar_kds=""; tar --keep-directory-symlink --version >/dev/null 2>&1 && tar_kds=--keep-directory-symlink
( cd "$b" && tar -cpf - $(ls -A) | tar $tar_kds -f - -xpC "$r" ) >/dev/null 2>&1
if [ -L "$r$t/lib" ]; then
	ok "restoring the backup left the symlink a symlink"
else
	fail "restoring the backup replaced the symlink with a directory"
fi
finish
