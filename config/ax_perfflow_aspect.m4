AC_DEFUN([AX_PERFFLOW_ASPECT], [
    PKG_CHECK_MODULES([$1], [perfflowaspect],
        [
            $1_WEAVEPASS=`pkg-config --variable=weavepass perfflowaspect`
            if test ! -e "${$1_WEAVEPASS}"; then
                AC_MSG_ERROR([Cannot find libWeavePass.so!])
            fi
            $1_PLUGIN_CPPFLAGS="-Xclang -load -Xclang ${$1_WEAVEPASS} -fPIC"
            AC_MSG_NOTICE([Flags are ${$1_PLUGIN_CPPFLAGS}])
            AC_SUBST($1_WEAVEPASS)
            AC_SUBST($1_PLUGIN_CPPFLAGS)
            $2
        ],
        [
            $3
        ]
    )
])
