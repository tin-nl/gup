#! /bin/bash

rm -rf .git
#cp -r ./* ~/q/nyh/gup/ 
#
#cat config.guess > ~/q/nyh/gup/config.guess
#cat config.sub   > ~/q/nyh/gup/config.sub

#rsync -r -I -U -t --copy-links -i -c --ignore-existing --stats --progress '--exclude=.git/' '--exclude=*.[ao]' '--exclude=*.l[ao]' '--exclude=*.bak' '--exclude=*.i' '--exclude=*~' '--exclude=autom4te.cache/*'   . ~/q/nyh/Atari/e/pc/gup/
#rsync -a -v -P --copy-links -c --stats --progress '--exclude=.git/' '--exclude=*.[ao]' '--exclude=*.l[ao]' '--exclude=*.bak' '--exclude=*.i' '--exclude=*~' '--exclude=autom4te.cache/*'   . ~/q/nyh/Atari/e/pc/gup/
rsync -a -v -P --copy-links -c --stats --progress    . ~/q/nyh/Atari/e/pc/gup/
#rsync -r -t --copy-links -c --stats --progress '--exclude=.git/'    . ~/q/nyh/Atari/e/pc/gup/
#cp -r .     ~/q/nyh/Atari/e/pc/gup/
