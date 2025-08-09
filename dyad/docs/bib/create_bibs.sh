#!/bin/bash

function usage {
    echo "Usage: ./create_bibs.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "--------"
    echo "  * -rst:          create RST bibliographies from .bib files. Enabled by default"
    echo "  * -md:           create Markdown bibliographies from .bib files"
    echo "  * --delete | -d: delete generated files instead of creating them"
    echo "  * --help | -h:   print this usage message"
}

enable_rst=1
enable_md=0
delete_mode=0

while test $# -ne 0; do
    case "$1" in
        -rst)
            enable_rst=1
            enable_md=0
            ;;
        -md)
            enable_md=1
            enable_rst=0
            ;;
        --delete | -d)
            delete_mode=1
            ;;
        --help | -h)
            usage
            exit 0
            ;;
    esac
    shift
done

if test $delete_mode -ne 0; then
    echo "Deleting the following files:"
else
    echo "Generating the following files:"
fi

if test $enable_rst -ne 0; then
    for bib_file in *.bib; do
        if test ! -f "$bib_file"; then
            continue
        fi
        out_file="${bib_file%%.*}.rst"
        if test $delete_mode -ne 0; then
            rm $out_file
            echo " * $out_file"
        else
            pandoc $bib_file --citeproc --csl ieee.csl -s -o $out_file && echo " * $out_file"
        fi
    done
fi

if test $enable_md -ne 0; then
    for bib_file in *.bib; do
        if test ! -f "$bib_file"; then
            continue
        fi
        out_file="${bib_file%%.*}.md"
        if test $delete_mode -ne 0; then
            rm $out_file
        else
            pandoc $bib_file --citeproc --csl ieee.csl -s -o $out_file && echo " * $out_file"
        fi
    done
fi
