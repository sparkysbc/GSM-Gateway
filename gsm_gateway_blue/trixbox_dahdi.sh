
if [ $# -le '0' ] ; then
    echo "Usage: ./trixbox_dahdi.sh trixbox_version logfile_name"
    exit 1
fi

PROD_HOME=`pwd`
ENVIRON_TRIXBOX_VERSION=$1
PROD_PATCH_DIR=$PROD_HOME/patches
AST_MODULES="/usr/lib/asterisk/modules"
OPVXG4XX_DIR=$PROD_HOME/src/allog4c
AGSM_DIR=$PROD_HOME/src/chan_allogsm
AST_CONFIG_DIR="/etc/asterisk"

# log file
#INSTALL_LOG="$PROD_HOME/log/trixbox_install.log.$(date +%F-%k-%M)"
INSTALL_LOG=$2

if [ "$ENVIRON_TRIXBOX_VERSION"x = "2.8.0.4"x ] ; then  #trixbox-2.8.0.4
    ASTERISK_SRC_RPM="asterisk16-1.6.0.26-1_trixbox.src.rpm"
    DAHDI_SRC_RPM="dahdi-linux-2.3.0.1-1_trixbox.src.rpm"
    DAHDI_SRC_DIR="/usr/src/redhat/SOURCES/dahdi-linux-2.3.0.1"
    DAHDI_GZ="/usr/src/redhat/SOURCES/dahdi-linux-sources-2.3.0.1.tar.gz"
    ASTERISK_SRC_DIR="/usr/src/redhat/SOURCES/asterisk16-1.6.0.26"
    ASTERISK_CODE_URL="http://yum.trixbox.org/trixbox/2.8/SRPMS/$ASTERISK_SRC_RPM"
    DAHDI_CODE_URL="http://yum.trixbox.org/trixbox/2.8/SRPMS/$DAHDI_SRC_RPM"
else #trixbox-2.8.0.1,trixbox-2.8.0.3
    ASTERISK_SRC_RPM="asterisk16-1.6.0.22-1_trixbox.src.rpm"
    DAHDI_SRC_RPM="dahdi-linux-2.2.0-4_trixbox.src.rpm"
    DAHDI_SRC_DIR="/usr/src/redhat/SOURCES/dahdi-linux-2.2.0"
    DAHDI_GZ="/usr/src/redhat/SOURCES/dahdi-linux-sources-2.2.0.tar.gz"
    ASTERISK_SRC_DIR="/usr/src/redhat/SOURCES/asterisk16-1.6.0.22"
    ASTERISK_CODE_URL="http://yum.trixbox.org/trixbox/2.8/SRPMS/$ASTERISK_SRC_RPM"
    DAHDI_CODE_URL="http://yum.trixbox.org/trixbox/2.8/SRPMS/$DAHDI_SRC_RPM"
fi

logger()
{
   echo $1 >> $INSTALL_LOG
}

backup_file()
{
	\cp -a "$1" "$1.$(date +%F-%k-%M)"
	if [ $? -ne 0 ]; then
		return 1
	fi
}

check_file()
{
	if [ ! -f "$1" ]; then
		echo "==========================================================================="
		echo "No such file : $1 "
		logger "No such file or directory : $1 "
		echo "==========================================================================="
		return 1
	fi
}

check_dir()
{
  if [ ! -d "$1" ]; then
		echo "==========================================================================="
		echo "No such directory : $1 "
		logger "No such file or directory : $1 "
		echo "==========================================================================="
		return 1
	fi
}

#download and uncompress_package
download_uncompress() 
{
	wget -c $ASTERISK_CODE_URL
	wget -c $DAHDI_CODE_URL
	wget -c http://google-coredumper.googlecode.com/files/coredumper-devel-1.2.1-1.i386.rpm
  
	mkdir -p /usr/src/redhat/SOURCES/
	rpm -ivh $DAHDI_SRC_RPM
	rpm -ivh $ASTERISK_SRC_RPM
	cd /usr/src/redhat/SOURCES/
	tar -zxf $DAHDI_GZ
	sleep 1
	tar -zxf $ASTERISK_SRC_DIR.tar.gz
	chmod -R 744 *
  
	eval "rpm -q coredumper > /dev/null"
	if [ $? -eq 0 ]; then
		echo
	else
		yes|yum install coredumper  
	fi 
  
	rpm -Uvh $PROD_HOME/coredumper-devel-1.2.1-1.i386.rpm
  
	eval "rpm -q spandsp-devel > /dev/null"
	if [ $? -eq 0 ]; then
		:
	else
		yes|yum install spandsp-devel
	fi
  
	if [ "$ENVIRON_TRIXBOX_VERSION"x = "2.8.0.4"x ] ; then
		eval "rpm -q gtk2-devel > /dev/null"
		if [ $? -eq 0 ]; then
			:
		else
			yes|yum install gtk2-devel
		fi
	fi

	cd $PROD_HOME
	sleep 1
}

#######################################################################################
# Create by Freedom 2011-11-23 08:52
#######################################################################################
DAHDI_VER_NUM=
get_dahdi_version()
{
	DAHDI_VER_NUM=`awk -F. '{printf "%d%02d%02d%02d", $1, $2, $3, $4}' $DAHDI_LINUX_COMPLETE_SOURCE_DIR/linux/.version`
}

install_allog4c()
{
	echo
	
	KBUILD_FILE="$DAHDI_SRC_DIR/drivers/dahdi/Kbuild"
	eval "\cp -rf $OPVXG4XX_DIR $DAHDI_SRC_DIR/drivers/dahdi/"
	
	######################################################################
	# Freedom add 2011-11-23 08:52
	######################################################################
	get_dahdi_version
	if [ "$DAHDI_VER_NUM" == "0000" ]; then
		echo "get dahdi version error\n"
		exit 1
	fi
	OPVXG4XX_H_FILE="$DAHDI_SRC_DIR/drivers/dahdi/allog4c/allog4c.h"
	sed -i "/#define _OPVXG4XX_H_/a#define DAHDI_VER_NUM $DAHDI_VER_NUM" $OPVXG4XX_H_FILE
	#######################################################################	

	eval "grep -q CONFIG_DAHDI_ALLOG4C $KBUILD_FILE"
	if [ $? -ne 0 ]; then
		backup_file "$KBUILD_FILE"
		sed -i "/CONFIG_DAHDI_TRANSCODE/a\obj-\$(DAHDI_BUILD_ALL)\$(CONFIG_DAHDI_OPVXG4XX)		+= allog4c/" $KBUILD_FILE
	fi
        
  sleep 1

	echo
	echo "========================================================================"	
	echo   "Installing ALLO GSM driver ...                             [ OK ]"
	logger "Installing ALLO GSM driver ...                             [ OK ]"
	echo "========================================================================"

	cd $PROD_HOME
	sleep 1
}


install_dahdi()
{
	echo
	
	cd $DAHDI_SRC_DIR/
	make >> "$INSTALL_LOG"
	if [ $? -ne 0 ]; then
		echo
		echo "ERROR: Installing $DAHDI_SRC_DIR: make ...                   [ FAILURE ]"
	fi
	
	mkdir -p /lib/modules/`uname -r`/dahdi/allog4c/
	\cp $DAHDI_SRC_DIR/drivers/dahdi/allog4c/allog4c.ko /lib/modules/`uname -r`/dahdi/allog4c/
	\cp -fr $DAHDI_SRC_DIR/include/dahdi /usr/include/dahdi/
	
	echo "copy tonezone.h from ./src/chan_allogsm/src/ to /usr/include/dahdi/"
	\cp $AGSM_DIR/src/tonezone.h /usr/include/dahdi/
	
	echo
	echo "========================================================================"
	echo   "Installing $DAHDI_SRC_DIR ...                                   [ OK ]"
	logger "Installing $DAHDI_SRC_DIR ...                                   [ OK ]"
	echo "========================================================================"
	
	cd $PROD_HOME
	sleep 1
}

patch_asterisk_configure()
{	

  #check file or directory 
  check_dir $ASTERISK_SRC_DIR
  check_file $PROD_HOME/patches/asterisk/configure.1.sed
  check_file $PROD_HOME/patches/asterisk/configure.2.sed
  check_file $PROD_HOME/patches/asterisk/configure.3.sed
  check_file $PROD_HOME/patches/asterisk/configure.4.sed
  check_file $PROD_HOME/patches/asterisk/configure.5.sed
  check_file $PROD_HOME/patches/asterisk/configure.ac.1.sed
  check_file $PROD_HOME/patches/asterisk/configure.ac.2.sed
  check_file $PROD_HOME/patches/asterisk/makeopts.in.sed
  check_file $PROD_HOME/patches/asterisk/menuselect-deps.in.sed
  check_file $PROD_HOME/patches/asterisk/autoconfig.h.in.sed
	
  eval "grep -q ALLOGSMAT_LIB $ASTERISK_SRC_DIR/configure"
	if [ $? -eq 0 ]; then
		return 0
	fi

	# backup asterisk configuration files
	backup_file $ASTERISK_SRC_DIR/configure
	backup_file $ASTERISK_SRC_DIR/configure.ac
	backup_file $ASTERISK_SRC_DIR/makeopts.in
	backup_file $ASTERISK_SRC_DIR/build_tools/menuselect-deps.in
	backup_file $ASTERISK_SRC_DIR/include/asterisk/autoconfig.h.in
	
	# add gsmat in configure 
	file_configure="$ASTERISK_SRC_DIR/configure"

	file_configure_1_sed="$PROD_HOME/patches/asterisk/configure.1.sed"
	linenumber=`sed -n '/ac_subst_vars=/=' $file_configure`
	linenumber=$(($linenumber+1))
	sed -i "$linenumber r $file_configure_1_sed" $file_configure	

	file_configure_2_sed="$PROD_HOME/patches/asterisk/configure.2.sed"
	linenumber=`sed -n '/PRI_DESCRIP=\"ISDN PRI\"/=' $file_configure`
	if [ "$linenumber" = "" ]; then
		echo "/if test \"${with_resample+set}\" = set; then/= no found\n"
		exit 1
	fi
	linenumber=$(($linenumber-1))
	sed -i "$linenumber r $file_configure_2_sed" $file_configure

	file_configure_3_sed="$PROD_HOME/patches/asterisk/configure.3.sed"

  linenumber=`sed -n '/if test \"x${PBX_RESAMPLE}\"/=' $file_configure` 	
	if [ "$linenumber" = "" ]; then
		echo "PRI_INBANDDISCONNECT_DESCRIP=\"ISDN PRI set_inbanddisconnect\" no found\n"
		exit 1
	fi
	linenumber=$(($linenumber-1))
	sed -i "$linenumber r $file_configure_3_sed" $file_configure

	file_configure_5_sed="$PROD_HOME/patches/asterisk/configure.5.sed"
	sed -i "/--with-pri=PATH/r $file_configure_5_sed" $file_configure

  if [ "$ENVIRON_TRIXBOX_VERSION"x = "2.8.0.4"x ] ; then
      file_configure_4_sed="$PROD_HOME/patches/asterisk/configure.4.sed"
	    sed -i "/PBX_PRI!\$PBX_PRI\$ac_delim/r $file_configure_4_sed" $file_configure
      
      linenumber=$(sed -n '/PBX_PRI!\$PBX_PRI\$ac_delim/=' $file_configure) 
    	linelist=$(sed -n -e '1{/if\ test\ `sed -n \"/p}' -e '/if\ test\ `sed -n \"/=' $file_configure)
    	line_arr=($linelist)
    	result=-1
    	for (( i=0;  i<${#line_arr[@]};  i=i+1 ))  #${#line_arr[@]} is the number of line_arr elements  
    	do
    		if [ "${line_arr[$i]}" -gt "$linenumber" ]; then
    		   result=${line_arr[$i]}
    		   break;
    		fi
    	done
    	content=$(sed -n ''$result'p' $file_configure)  #### if test `sed -n "s/.*$ac_delim\$/X/p" conf$$subs.sed | grep -c X` = 101; then
    	oldno=$(echo $content | grep -Eo '[0-9]+') 
    	newno=`expr $oldno + 4`
    	sed -i "$result s/$oldno/$newno/" $file_configure
  fi  
  
	# add gsmat in autoconfig.h.in 
	file_autocfg="$ASTERISK_SRC_DIR/include/asterisk/autoconfig.h.in"
	file_autocfg_sed="$PROD_HOME/patches/asterisk/autoconfig.h.in.sed"
	sed -i "/#undef HAVE_PRI_INBANDDISCONNECT/r $file_autocfg_sed" $file_autocfg

  # add gsmat in configure.ac
	file_ac="$ASTERISK_SRC_DIR/configure.ac"
	file_ac_1_sed="$PROD_HOME/patches/asterisk/configure.ac.1.sed"
	file_ac_2_sed="$PROD_HOME/patches/asterisk/configure.ac.2.sed"
	sed -i "/AST_EXT_LIB_SETUP(\[PRI\], \[ISDN PRI\], \[pri\])/r $file_ac_1_sed" $file_ac
	sed -i "/AST_EXT_LIB_CHECK(\[PRI_INBANDDISCONNECT\], \[pri\], \[pri_set_inbanddisconnect\], \[libpri.h\])/r $file_ac_2_sed" $file_ac

   # add gsmat in makeopts.in
	file_opt="$ASTERISK_SRC_DIR/makeopts.in"
	file_opt_sed="$PROD_HOME/patches/asterisk/makeopts.in.sed"
	sed -i "/PRI_LIB=@PRI_LIB@/r $file_opt_sed" $file_opt

  # add gsmat in menuselect-deps.in
	file_menu="$ASTERISK_SRC_DIR/build_tools/menuselect-deps.in"
	file_menu_sed="$PROD_HOME/patches/asterisk/menuselect-deps.in.sed"
	sed -i "/PRI=@PBX_PRI@/r $file_menu_sed" $file_menu

	echo
	
	echo "========================================================================"
	echo "Patching asterisk configure files ...                             [ OK ]"
	echo "========================================================================"
	
	cd $PROD_HOME

	sleep 1
}

install_asterisk()
{
	echo
	
	\cp $AGSM_DIR/src/chan_allogsm.c $ASTERISK_SRC_DIR/channels/chan_allogsm.c
	\cp $AGSM_DIR/config/chan_allogsm.conf $AST_CONFIG_DIR
	\cp $AGSM_DIR/config/allogsm-channels.conf $AST_CONFIG_DIR
	\cp $AGSM_DIR/src/libpri_1.4.10-1.h /usr/include/libpri.h
	
	backup_file "/usr/lib/asterisk/modules"
	
	cd $ASTERISK_SRC_DIR
	
	####### modify Makefile 
	makefile="$ASTERISK_SRC_DIR/Makefile"
	backup_file $makefile
	linenumber=`sed -n '/ASTMANDIR=\$(mandir)/=' $makefile`
	linenumber=$(($linenumber-1))
	sed -i "$linenumber"d $makefile
	sleep 1
	sed -i '/ASTMANDIR=\$(mandir)/i \  ASTVARRUNDIR=\$(localstatedir)/run/asterisk' $makefile
	
	touch .cleancount
	
	echo "Installing asterisk, it will takes a few minutes, please wait..."
	
	./configure >> "$INSTALL_LOG"
	if [ $? -ne 0 ]; then
		echo
		echo "ERROR: Installing Asterisk: ./configure ...                   [ FAILURE ]"
	fi
	
	make >> "$INSTALL_LOG"
	if [ $? -ne 0 ]; then
		echo
		echo "ERROR: Installing $ASTERISK_SRC_DIR: make ...                   [ FAILURE ]"
		exit 1
	fi
	
	make install >> "$INSTALL_LOG"
	if [ $? -ne 0 ]; then
		echo
		echo "ERROR: Installing $ASTERISK_SRC_DIR: make install...                   [ FAILURE ]"
		exit 1
	fi
	
	#\cp $ASTERISK_SRC_DIR/channels/chan_allogsm.so $AST_MODULES
	
	echo
	echo "========================================================================"
	echo   "Installing Asterisk ...                                         [ OK ]"
	logger "Installing Asterisk ...                                         [ OK ]"
	echo "========================================================================"
	
	cd $PROD_HOME
	sleep 1
}


check_dahdi_tool_config_files()
{      
	local flag
	flag=0	
	
	for file in $*
	do
		check_file "$file"
		if [ $? -ne 0 ]; then
			flag=1
		fi
	done

	if [ $flag -eq 1 ]; then
		exit 1
	fi

	echo
	echo "========================================================================"
	echo   "Checking dahdi_tool configuration files ...                     [ OK ]"
	logger "Checking dahdi_tool configuration files ...                     [ OK ]"
	echo "========================================================================"
	
	cd "$PROD_HOME"

	sleep 1
}

check_dahdi_tool_patch_files()
{
	local flag
	flag=0
	
	if [ ! -d "$DAHDI_SRC_DIR" ]; then
		error "$DAHDI_SRC_DIR not found\n"
		return 1
	fi
	
	if [ ! -d "$PROD_PATCH_DIR/dahdi" ]; then
		error "$PROD_PATCH_DIR/dahdi not found\n"
		return 1
	fi

	cd "$PROD_PATCH_DIR/dahdi"
	dahdi_tool_patch_files="Chandahdi.pm.sed"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""Chanallogsm.pm"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""Span.pm.1.sed Span.pm.2.sed Span.pm.3.sed"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""dahdi_genconf.sed"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""Gen.pm.sed"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""Params.pm.sed"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""PCI.pm.sed"
	dahdi_tool_patch_files="$dahdi_tool_patch_files ""modules.sample.sed"
	
	for file in $dahdi_tool_patch_files
	do
		check_file "$PROD_PATCH_DIR/dahdi/$file"
		if [ $? -ne 0 ]; then
			flag=1
		fi
	done

	if [ $flag -eq 1 ]; then
		exit 1
	fi

	echo
	echo "========================================================================"
	echo   "Checking dahdi_tool patch files ...                             [ OK ]" 
	logger "Checking dahdi_tool patch files ...                             [ OK ]"
	echo "========================================================================"
	
	cd "$PROD_HOME"

	sleep 1

}

backup_dahdi_tool_config_files()
{
	local flag
	flag=0

	for file in $*
	do
		backup_file "$file"
		if [ $? -ne 0 ]; then
			flag=1
		fi
	done

	if [ $flag -eq 1 ]; then
		exit 1
	fi

	echo
	echo "========================================================================"
	echo   "Backuping dahdi_tool configuration files ...                    [ OK ]"
	logger "Backuping dahdi_tool configuration files ...                    [ OK ]"
	echo "========================================================================"
	
	cd "$PROD_HOME"

	sleep 1
}

patch_dahdi_tool()
{
	if [ $DAHDI_SRC_DIR == "" ]; then
		echo "Error : DAHDI SOURCE DIR NOT FOUND !"
		exit 1
	fi

	DAHDI_TOOL_CONFIG_FILES="/usr/share/doc/dahdi-tools-doc-2.2.0/modules.sample"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Params.pm"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen.pm"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/sbin/dahdi_genconf"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/lib/perl5/site_perl/5.8.8/Dahdi/Span.pm"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/System.pm"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/Chandahdi.pm"
 	#DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""tools/xpp/perl_modules/Dahdi/Config/Gen/Chanallogsm.pm"
	DAHDI_TOOL_CONFIG_FILES="$DAHDI_TOOL_CONFIG_FILES ""/usr/lib/perl5/site_perl/5.8.8/Dahdi/Hardware/PCI.pm"

	# check dahdi_tool configuration files
	check_dahdi_tool_config_files $DAHDI_TOOL_CONFIG_FILES
	
	# check *.sed file
	check_dahdi_tool_patch_files

	# backup asterisk configuration files
	backup_dahdi_tool_config_files $DAHDI_TOOL_CONFIG_FILES

	#patch /etc/dahdi/modules
	file_modules="/usr/share/doc/dahdi-tools-doc-2.2.0/modules.sample"
	grep -q "^allog4c" $file_modules
	if [ $? -ne 0 ]; then
		file_modules_sample_sed="$PROD_PATCH_DIR/dahdi/modules.sample.sed"
		sed -i "/^xpp_usb/r $file_modules_sample_sed" $file_modules
	fi
	
	#patch /etc/dahdi/modules
	file_modules="/etc/dahdi/modules"
	grep -q "^allog4c" $file_modules
	if [ $? -ne 0 ]; then
		file_modules_sample_sed="$PROD_PATCH_DIR/dahdi/modules.sample.sed"
		sed -i "/^xpp_usb/r $file_modules_sample_sed" $file_modules
	fi

	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Params.pm
	file_params_pm="/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Params.pm"
	grep -q "\bgsm_type\b" $file_params_pm
	if [ $? -ne 0 ]; then
		file_params_pm_sed="$PROD_PATCH_DIR/dahdi/Params.pm.sed"
		sed -i "/^\s\+pri_connection_type/r $file_params_pm_sed" $file_params_pm
	fi
	
	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen.pm
	file_gen_pm="/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen.pm"
	grep -q "\bgsm_type\b" $file_gen_pm
	if [ $? -ne 0 ]; then
		file_gen_pm_sed="$PROD_PATCH_DIR/dahdi/Gen.pm.sed"
		sed -i "/^\s\+pri_connection_type/r $file_gen_pm_sed" $file_gen_pm
	fi
	
	#patch /usr/sbin/dahdi_genconf
	file_dahdi_genconf="/usr/sbin/dahdi_genconf"
	grep -q "push @genlist, 'chanallogsm'" $file_dahdi_genconf
	if [ $? -ne 0 ]; then
		file_dahdi_genconf_sed="$PROD_PATCH_DIR/dahdi/dahdi_genconf.sed"
		sed -i "/@genlist = ('system', 'chandahdi')/r $file_dahdi_genconf_sed" $file_dahdi_genconf
	fi

	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Span.pm
	file_span_pm="/usr/lib/perl5/site_perl/5.8.8/Dahdi/Span.pm"
	grep -q "AlloGSM A4G" $file_span_pm
	if [ $? -ne 0 ]; then
		file_span_pm_1_sed="$PROD_PATCH_DIR/dahdi/Span.pm.1.sed"
		sed -i "/our \$DAHDI_PRI_CPE = 'pri_cpe'/r $file_span_pm_1_sed" $file_span_pm
	fi
	grep -q "foreach my \$cardtype (@gsm_strings)" $file_span_pm
	if [ $? -ne 0 ]; then
		file_span_pm_2_sed="$PROD_PATCH_DIR/dahdi/Span.pm.2.sed"
		#sed -i "/\$self->{IS_PRI} = 0/r $file_span_pm_2_sed" $file_span_pm
		sed -i "/\$self->{TERMTYPE} = \"UNKNOWN\"/r $file_span_pm_2_sed" $file_span_pm
	fi
	grep -q "if(\$chan_fqn =~ m(allog4c" $file_span_pm
	if [ $? -ne 0 ]; then
		file_span_pm_3_sed="$PROD_PATCH_DIR/dahdi/Span.pm.3.sed"
		sed -i "/\$self->{CRC4} = undef/r $file_span_pm_3_sed" $file_span_pm
	fi

	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/system.pm
	file_system_pm="/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/System.pm"
	grep -q "\$span->is_gsm" $file_system_pm
	if [ $? -ne 0 ]; then
		sed -i "s/\$span->is_bri()/\$span->is_bri()|\$span->is_gsm()/g" $file_system_pm
		sed -i "s/(\$span->is_bri)/(\$span->is_bri|\$span->is_gsm)/g" $file_system_pm
	fi

	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/Chandahdi.pm
	file_chandahdi_pm="/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/Chandahdi.pm"
	grep -q "if(\$span->is_gsm)" $file_chandahdi_pm
	if [ $? -ne 0 ]; then
		file_chandahdi_pm_sed="$PROD_PATCH_DIR/dahdi/Chandahdi.pm.sed"
		sed -i "/foreach my \$span (@spans)/r $file_chandahdi_pm_sed" $file_chandahdi_pm
	fi
	
	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/chanallogsm.pm
	\cp "$PROD_PATCH_DIR/dahdi/Chanallogsm.pm" "/usr/lib/perl5/site_perl/5.8.8/Dahdi/Config/Gen/Chanallogsm.pm"

	#patch /usr/lib/perl5/site_perl/5.8.8/Dahdi/Hardware/PCI.pm
	file_pci_pm="/usr/lib/perl5/site_perl/5.8.8/Dahdi/Hardware/PCI.pm"
	grep -q "10b5:d44f" $file_pci_pm
	if [ $? -ne 0 ]; then
		file_pci_pm_sed="$PROD_PATCH_DIR/dahdi/PCI.pm.sed"
		sed -i "/my %pci_ids = (/r $file_pci_pm_sed" $file_pci_pm
	fi
	

	return 0
}

dahdi_black_list()
{
	dahdi_black="/etc/modprobe.d/dahdi.blacklist"
	grep -q "allog4c" $dahdi_black
	if [ $? -ne 0 ]; then
		backup_file $dahdi_black
		echo "blacklist allog4c" >> $dahdi_black
	fi
}

main()
{
  download_uncompress  
  install_allog4c
  install_dahdi
  patch_asterisk_configure
  install_asterisk
  patch_dahdi_tool
  dahdi_black_list
}

main
