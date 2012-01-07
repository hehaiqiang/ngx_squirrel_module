
# Copyright (C) Ngwsx


if [ $NGX_SQU_DLL = YES ]; then
    squ_module_dir="$NGX_OBJS${ngx_dirsep}modules${ngx_dirsep}"
    mkdir -p $squ_module_dir

    ngx_cc="\$(CC) $ngx_compile_opt \$(CFLAGS) -DNGX_DLL=1 \$(ALL_INCS)"

    if [ "$NGX_PLATFORM" != win32 ]; then
        ngx_cc="$ngx_cc -fPIC"
        squ_module_link="-shared -fPIC"
    else
        nginx_lib="$NGX_OBJS${ngx_dirsep}nginx.lib"
        squ_module_def="$ngx_addon_dir/src/core/ngx_squ_module.def"
        squ_module_link="-link -dll -verbose:lib -def:$squ_module_def"
        squ_module_def_libs="ws2_32.lib $nginx_lib"
    fi

    squ_modules=""


    if [ $NGX_SQU_AUTORUN = YES ]; then
        squ_module="$NGX_SQU_AUTORUN_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_AUTORUN_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_AXIS2C = YES ]; then
        squ_module="$NGX_SQU_AXIS2C_MODULE"
        squ_module_libs="$squ_module_def_libs $AXIS2C_LIBS"
        squ_module_incs=
        squ_module_deps="$ngx_cont$NGX_SQU_AXIS2C_DEPS"
        squ_module_srcs="$NGX_SQU_AXIS2C_SRCS"
        if [ $NGX_SQU_AXIS2C_WS = YES ]; then
            squ_module_srcs="$squ_module_srcs $NGX_SQU_AXIS2C_WS_SRCS"
        fi
        if [ $NGX_SQU_AXIS2C_XML = YES ]; then
            squ_module_srcs="$squ_module_srcs $NGX_SQU_AXIS2C_XML_SRCS"
        fi
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_DAHUA = YES ]; then
        squ_module="$NGX_SQU_DAHUA_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_DAHUA_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_DBD = YES ]; then
        squ_module="$NGX_SQU_DBD_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs="$NGX_SQU_DBD_INCS"
        squ_module_deps="$ngx_cont$NGX_SQU_DBD_DEPS"
        squ_module_srcs="$NGX_SQU_DBD_MODULE_SRCS"
        . $ngx_addon_dir/auto/make

        if [ $NGX_SQU_DBD_LIBDRIZZLE = YES ]; then
            squ_module="$NGX_SQU_DBD_LIBDRIZZLE_MODULE"
            squ_module_libs="$squ_module_def_libs user32.lib $LIBDRIZZLE_LIBS"
            squ_module_incs="$NGX_SQU_DBD_INCS"
            squ_module_deps="$ngx_cont$NGX_SQU_DBD_DEPS"
            squ_module_srcs="$NGX_SQU_DBD_LIBDRIZZLE_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_DBD_SQLITE3 = YES ]; then
            squ_module="$NGX_SQU_DBD_SQLITE3_MODULE"
            squ_module_libs="$squ_module_def_libs $SQLITE3_LIBS"
            squ_module_incs="$NGX_SQU_DBD_INCS"
            squ_module_deps="$ngx_cont$NGX_SQU_DBD_DEPS"
            squ_module_srcs="$NGX_SQU_DBD_SQLITE3_SRCS"
            . $ngx_addon_dir/auto/make
        fi
    fi

    if [ $NGX_SQU_FILE = YES ]; then
        squ_module="$NGX_SQU_FILE_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_FILE_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_LOGGER = YES ]; then
        squ_module="$NGX_SQU_LOGGER_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_LOGGER_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_SMTP = YES ]; then
        squ_module="$NGX_SQU_SMTP_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_SMTP_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_SOCKET = YES ]; then
        squ_module="$NGX_SQU_SOCKET_MODULE"
        squ_module_libs="$squ_module_def_libs"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_SOCKET_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_UTILS = YES ]; then
        squ_module="$NGX_SQU_UTILS_MODULE"
        squ_module_libs="$squ_module_def_libs $SHA1_LIBS"
        squ_module_incs=
        squ_module_deps=
        squ_module_srcs="$NGX_SQU_UTILS_SRCS"
        . $ngx_addon_dir/auto/make
    fi

    if [ $NGX_SQU_HTTP = YES -o $NGX_SQU_HTTP_LOG = YES ]; then
        if [ $NGX_SQU_HTTP_REQUEST = YES ]; then
            squ_module="$NGX_SQU_HTTP_REQUEST_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_HTTP_REQUEST_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_HTTP_RESPONSE = YES ]; then
            squ_module="$NGX_SQU_HTTP_RESPONSE_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_HTTP_RESPONSE_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_HTTP_SESSION = YES ]; then
            squ_module="$NGX_SQU_HTTP_SESSION_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps="$ngx_cont$NGX_SQU_HTTP_SESSION_DEPS"
            squ_module_srcs="$NGX_SQU_HTTP_SESSION_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_HTTP_VARIABLE = YES ]; then
            squ_module="$NGX_SQU_HTTP_VARIABLE_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_HTTP_VARIABLE_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_BTT = YES -a $NGX_SQU_HTTP_BTT = YES ]; then
            squ_module="$NGX_SQU_HTTP_BTT_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_HTTP_BTT_SRCS"
            . $ngx_addon_dir/auto/make
        fi
    fi

    if [ $NGX_SQU_TCP = YES ]; then
        if [ $NGX_SQU_TCP_REQUEST = YES ]; then
            squ_module="$NGX_SQU_TCP_REQUEST_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_TCP_REQUEST_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_TCP_RESPONSE = YES ]; then
            squ_module="$NGX_SQU_TCP_RESPONSE_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_TCP_RESPONSE_SRCS"
            . $ngx_addon_dir/auto/make
        fi
    fi

    if [ $NGX_SQU_UDP = YES ]; then
        if [ $NGX_SQU_UDP_REQUEST = YES ]; then
            squ_module="$NGX_SQU_UDP_REQUEST_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_UDP_REQUEST_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_BTT = YES -a $NGX_SQU_UDP_BTT = YES ]; then
            squ_module="$NGX_SQU_UDP_BTT_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_UDP_BTT_SRCS"
            . $ngx_addon_dir/auto/make
        fi
    fi

    if [ $NGX_SQU_UDT = YES ]; then
        if [ $NGX_SQU_UDT_REQUEST = YES ]; then
            squ_module="$NGX_SQU_UDT_REQUEST_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_UDT_REQUEST_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_UDT_RESPONSE = YES ]; then
            squ_module="$NGX_SQU_UDT_RESPONSE_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_UDT_RESPONSE_SRCS"
            . $ngx_addon_dir/auto/make
        fi

        if [ $NGX_SQU_BTT = YES -a $NGX_SQU_UDT_BTT = YES ]; then
            squ_module="$NGX_SQU_UDT_BTT_MODULE"
            squ_module_libs="$squ_module_def_libs"
            squ_module_incs=
            squ_module_deps=
            squ_module_srcs="$NGX_SQU_UDT_BTT_SRCS"
            . $ngx_addon_dir/auto/make
        fi
    fi


    cat << END                                                >> $NGX_MAKEFILE

modules:	$squ_modules

END

fi
