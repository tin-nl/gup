#! /bin/bash

#cp -r ~/q/nyh/gup/ ../ ; rm -rf .git
rsync -v -r --exclude=.git/ '--exclude=*.[ao]' '--exclude=*.l[ao]' '--exclude=*.bak' '--exclude=*.i' '--exclude=*~' '--exclude=autom4te.cache/*' -u -t -U --copy-links  ~/q/nyh/gup/ ./
