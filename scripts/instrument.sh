#!/bin/sh

if echo "$1" | grep -q '\.set$'; then
	FILES=`cat "$1"`
else
	FILES="$1"
fi

for FILE in $FILES; do
	sed -i -e '
	s@^\s*\<ERROR:[ ;]*$@& assert(0);@;
	s@^\s*\<ERROR: *return@ERROR: assert(0); return@;
	s@^\s*{* *ERROR: goto ERROR; *}*\s*$@{ ERROR: assert(0); goto ERROR; }@;
	s@^\s*\(if (.*)\) ERROR: goto ERROR;;*\s*$@\1 { ERROR: assert(0); goto ERROR; }@;
	s@^\(void __VERIFIER_assert(int expression, char\* x) {\) if (!expression) { ERROR: goto ERROR; }; return; }$@\1 assert(expression); }@;
	s@;\s*\<ERROR:@& assert(0);@' "$FILE"
done
