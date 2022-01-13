#!/bin/bash
pandoc -f markdown -t html5 ../README.md -o ../readme.html --lua-filter=links-to-html.lua
pandoc -f markdown -t html5 ../TESTED.md -o ../tested.html --lua-filter=links-to-html.lua
pandoc -f markdown -t html5 ../CHANGELOG.md -o ../changelog.html --lua-filter=links-to-html.lua
