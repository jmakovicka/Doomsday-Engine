#!/bin/bash
################################################################################
#  Copyright and License Summary
#  License: GPL
#  Online License Link: http://www.gnu.org/licenses/gpl.html
#  
#  Copyright © 2006 Jamie Jones <yagisan@dengine.net>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, 
#  Boston, MA  02110-1301  USA
################################################################################
#  
#  This script is used to do a license audit on sources in The Doomsday Engine.
#  It produces several files depending on the license of code it finds.
#  Currently it identifiles GPL, GPL + jHeretic/jHexen exception and Raven.
#  
################################################################################
FILES_PROCESSED=0
GPL_SOURCE_FILES=0
GPL_ONLY_SOURCE_FILES=0
GPL_PLUS_EXCEPTION_SOURCE_FILES=0
RAVEN_SOURCE_FILES=0
UNKNOWN_SOURCE_FILES=0
TOP_LEVEL_DIR=$PWD

findfiles()
{
find -name *.c > $TOP_LEVEL_DIR/filelist.txt
find -name *.cpp >> $TOP_LEVEL_DIR/filelist.txt
find -name *.h >> $TOP_LEVEL_DIR/filelist.txt
find -name *.m >> $TOP_LEVEL_DIR/filelist.txt
FILE_LIST=`cat $TOP_LEVEL_DIR/filelist.txt`
}
findprojectfiles()
{
find -name *.c | grep engine > $TOP_LEVEL_DIR/filelist.txt
find -name *.c | grep plugins >> $TOP_LEVEL_DIR/filelist.txt
find -name *.cpp | grep engine  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.cpp | grep plugins  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.h | grep engine  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.h | grep plugins  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.m | grep engine  >> $TOP_LEVEL_DIR/filelist.txt
find -name *.m | grep plugins  >> $TOP_LEVEL_DIR/filelist.txt

FILE_LIST=`cat $TOP_LEVEL_DIR/filelist.txt`
}

scanfiles()
{
let FILES_PROCESSED=0
let GPL_SOURCE_FILES=0
let GPL_ONLY_SOURCE_FILES=0
let GPL_PLUS_EXCEPTION_SOURCE_FILES=0
let RAVEN_SOURCE_FILES=0
let UNKNOWN_SOURCE_FILES=0
echo "<table>" > $TOP_LEVEL_DIR/code.html
echo "<tr>" >> $TOP_LEVEL_DIR/code.html
echo "<th colspan=\"2\" align=\"center\">" >> $TOP_LEVEL_DIR/code.html
echo "Individual file license details" >> $TOP_LEVEL_DIR/code.html
echo "</th>" >> $TOP_LEVEL_DIR/code.html
echo "</tr>" >> $TOP_LEVEL_DIR/code.html
echo "<tr>" >> $TOP_LEVEL_DIR/code.html
echo "<th>" >> $TOP_LEVEL_DIR/code.html
echo "Filename" >> $TOP_LEVEL_DIR/code.html
echo "</th>" >> $TOP_LEVEL_DIR/code.html
echo "<th>" >> $TOP_LEVEL_DIR/code.html
echo "License" >> $TOP_LEVEL_DIR/code.html
echo "</th>" >> $TOP_LEVEL_DIR/code.html
echo "</tr>" >> $TOP_LEVEL_DIR/code.html
for CURRENT_FILE in $FILE_LIST ;
do
#	echo $CURRENT_FILE
	grep -q " * License: GPL + jHeretic/jHexen Exception" $CURRENT_FILE 
	if [[ "$?" = 0 ]]
		then let GPL_SOURCE_FILES=GPL_SOURCE_FILES+1
		let GPL_PLUS_EXCEPTION_SOURCE_FILES=GPL_PLUS_EXCEPTION_SOURCE_FILES+1
		echo "<tr>" >> $TOP_LEVEL_DIR/code.html
		echo "<td>" >> $TOP_LEVEL_DIR/code.html
		echo $CURRENT_FILE >> $TOP_LEVEL_DIR/code.html
		echo "</td>" >> $TOP_LEVEL_DIR/code.html
		echo "<td>" >> $TOP_LEVEL_DIR/code.html
		echo "GPL + jHeretic/jHexen Exception" >> $TOP_LEVEL_DIR/code.html
		echo "</td>" >> $TOP_LEVEL_DIR/code.html
		echo "</tr>" >> $TOP_LEVEL_DIR/code.html
		else
		grep -q " * License: GPL" $CURRENT_FILE 
		if [[ "$?" = 0 ]]
			then let GPL_SOURCE_FILES=GPL_SOURCE_FILES+1
			let GPL_ONLY_SOURCE_FILES=GPL_ONLY_SOURCE_FILES+1
			echo "<tr>" >> $TOP_LEVEL_DIR/code.html
			echo "<td>" >> $TOP_LEVEL_DIR/code.html
			echo $CURRENT_FILE >> $TOP_LEVEL_DIR/code.html
			echo "</td>" >> $TOP_LEVEL_DIR/code.html
			echo "<td>" >> $TOP_LEVEL_DIR/code.html
			echo "GPL" >> $TOP_LEVEL_DIR/code.html
			echo "</td>" >> $TOP_LEVEL_DIR/code.html
			echo "</tr>" >> $TOP_LEVEL_DIR/code.html
			else
			grep -q " * License: Raven" $CURRENT_FILE 
			if [[ "$?" = 0 ]]
				then let RAVEN_SOURCE_FILES=RAVEN_SOURCE_FILES+1
				echo "<tr bgcolor=\"#0000ff\">" >> $TOP_LEVEL_DIR/code.html
				echo "<td>" >> $TOP_LEVEL_DIR/code.html
				echo $CURRENT_FILE >> $TOP_LEVEL_DIR/code.html
				echo "</td>" >> $TOP_LEVEL_DIR/code.html
				echo "<td>" >> $TOP_LEVEL_DIR/code.html
				echo "Raven" >> $TOP_LEVEL_DIR/code.html
				echo "</td>" >> $TOP_LEVEL_DIR/code.html
				echo "</tr>" >> $TOP_LEVEL_DIR/code.html
				else let UNKNOWN_SOURCE_FILES=UNKNOWN_SOURCE_FILES+1
				echo "<tr bgcolor=\"#ff0000\">" >> $TOP_LEVEL_DIR/code.html
				echo "<td>" >> $TOP_LEVEL_DIR/code.html
				echo $CURRENT_FILE >> $TOP_LEVEL_DIR/code.html
				echo "</td>" >> $TOP_LEVEL_DIR/code.html
				echo "<td>" >> $TOP_LEVEL_DIR/code.html
				echo "Autogenerated, Unknown or Unaudited" >> $TOP_LEVEL_DIR/code.html
				echo "</td>" >> $TOP_LEVEL_DIR/code.html
				echo "</tr>" >> $TOP_LEVEL_DIR/code.html
			fi
		fi
	fi
	let FILES_PROCESSED=FILES_PROCESSED+1
done
echo "</table>" >> $TOP_LEVEL_DIR/code.html
}

consoleout()
{
echo "Current Directory is: "$PWD
echo "Total Files Processed in this Module: "$FILES_PROCESSED
echo "This modules license structure is:"
echo ""
echo "Total GPL Only Code is                                     "$GPL_ONLY_SOURCE_PERCENT"% (" $GPL_ONLY_SOURCE_FILES" files )"
echo "Total GPL + jHeretic/jHexen Exception Code is              "$GPL_PLUS_EXCEPTION_SOURCE_PERCENT"% (" $GPL_PLUS_EXCEPTION_SOURCE_FILES" files )"
echo "------------------------------------------------------------------------"
echo "Combined GPL Compatible Total is                           "$GPL_SOURCE_PERCENT"% (" $GPL_SOURCE_FILES" files )"
echo "Total Raven Licensed Code is                               "$RAVEN_SOURCE_PERCENT"% (" $RAVEN_SOURCE_FILES" files )"
echo "Total Autogenerated, Unknown Licensed or Unaudited Code is "$UNKNOWN_SOURCE_PERCENT"% ( "$UNKNOWN_SOURCE_FILES" files )"
echo "------------------------------------------------------------------------"
echo "                                                           "$AUDIT_TOTAL_SANITY_CHECK
}

calcpercent()
{
GPL_ONLY_SOURCE_PERCENT=`echo "scale=3; 100/$FILES_PROCESSED*$GPL_ONLY_SOURCE_FILES"|bc -l`
GPL_PLUS_EXCEPTION_SOURCE_PERCENT=`echo "scale=3;  100/$FILES_PROCESSED*$GPL_PLUS_EXCEPTION_SOURCE_FILES"|bc -l`
GPL_SOURCE_PERCENT=`echo "scale=3;  100/$FILES_PROCESSED*$GPL_SOURCE_FILES"|bc -l`
RAVEN_SOURCE_PERCENT=`echo "scale=3;  100/$FILES_PROCESSED*$RAVEN_SOURCE_FILES"|bc -l`
UNKNOWN_SOURCE_PERCENT=`echo "scale=3;  100/$FILES_PROCESSED*$UNKNOWN_SOURCE_FILES"|bc -l`
AUDIT_TOTAL_SANITY_CHECK=`echo "scale=3; $GPL_SOURCE_PERCENT+$RAVEN_SOURCE_PERCENT+$UNKNOWN_SOURCE_PERCENT"|bc -l`
}

htmlsummary()
{

echo "<table>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<th colspan=\"3\">" >> $TOP_LEVEL_DIR/index.html
echo "Current Directory is: "$PWD >> $TOP_LEVEL_DIR/index.html
echo "</th>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html


echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td colspan=\"2\">" >> $TOP_LEVEL_DIR/index.html
echo "Total Files Processed in this Module: " >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $FILES_PROCESSED >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td colspan=\"3\">" >> $TOP_LEVEL_DIR/index.html
echo "This modules license structure is:" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td colspan=\"3\">" >> $TOP_LEVEL_DIR/index.html
echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<th>" >> $TOP_LEVEL_DIR/index.html
echo "License" >> $TOP_LEVEL_DIR/index.html
echo "</th>" >> $TOP_LEVEL_DIR/index.html
echo "<th>" >> $TOP_LEVEL_DIR/index.html
echo "Percentage of files" >> $TOP_LEVEL_DIR/index.html
echo "</th>" >> $TOP_LEVEL_DIR/index.html
echo "<th>" >> $TOP_LEVEL_DIR/index.html
echo "Number of files" >> $TOP_LEVEL_DIR/index.html
echo "</th>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td>" >> $TOP_LEVEL_DIR/index.html
echo "Total GPL Only Code is" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $GPL_ONLY_SOURCE_PERCENT"%" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $GPL_ONLY_SOURCE_FILES >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td>" >> $TOP_LEVEL_DIR/index.html
echo "Total GPL + jHeretic/jHexen Exception Code is" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $GPL_PLUS_EXCEPTION_SOURCE_PERCENT"%" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $GPL_PLUS_EXCEPTION_SOURCE_FILES >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td colspan=\"3\">" >> $TOP_LEVEL_DIR/index.html
echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html


echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td>" >> $TOP_LEVEL_DIR/index.html
echo "Combined GPL Compatible Total is" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $GPL_SOURCE_PERCENT"%" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $GPL_SOURCE_FILES >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td>" >> $TOP_LEVEL_DIR/index.html
echo "Total Raven Licensed Code is" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $RAVEN_SOURCE_PERCENT"%" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $RAVEN_SOURCE_FILES >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "<tr>" >> $TOP_LEVEL_DIR/index.html
echo "<td>" >> $TOP_LEVEL_DIR/index.html
echo "Total Autogenerated, Unknown Licensed or Unaudited Code is" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $UNKNOWN_SOURCE_PERCENT"%" >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "<td align=right>" >> $TOP_LEVEL_DIR/index.html
echo $UNKNOWN_SOURCE_FILES >> $TOP_LEVEL_DIR/index.html
echo "</td>" >> $TOP_LEVEL_DIR/index.html
echo "</tr>" >> $TOP_LEVEL_DIR/index.html

echo "</table>" >> $TOP_LEVEL_DIR/index.html
}

htmllist()
{
cat $TOP_LEVEL_DIR/code.html >> $TOP_LEVEL_DIR/index.html
}

auditall()
{
echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>Entire Project Audit Summary</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR
findprojectfiles
scanfiles
calcpercent
consoleout
htmlsummary

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"engine\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>The Doomsday Engine</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/engine
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"plugins\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>All Plugins Summary</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"jdoom\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>jDoom plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/jdoom
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"jheretic\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>jHeretic plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/jheretic
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"jhexen\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>jHexen plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/jhexen
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"wolftc\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>WolfTC plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/wolftc
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"doom64tc\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>Doom64TC plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/doom64tc
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"dehread\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>dehread plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/dehread
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"mapload\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>mapload plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/mapload
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"opengl\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>OpenGL plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/opengl
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"d3d\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>Direct3D plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/d3d
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist


echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"debugrenderer\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>debugrenderer plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/debugrenderer
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"openal\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>OpenAL plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/openal
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"sdlmixer\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>sdlmixer plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/sdlmixer
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"ds6\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>DirectSound Plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/ds6
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"a3d\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>a3d plugin</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/a3d
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist

echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"common\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>Plugin Common files</h1>" >> $TOP_LEVEL_DIR/index.html
cd $TOP_LEVEL_DIR/plugins/common
findfiles
scanfiles
calcpercent
consoleout
htmlsummary
echo "<a href=\"#top\">Return to Table of Contents</a>" >> $TOP_LEVEL_DIR/index.html
htmllist
}


toc()
{
echo "<hr>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>Contents</h1>" >> $TOP_LEVEL_DIR/index.html
echo "<ul>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#engine\">The Doomsday Engine</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#plugins\">All Plugins Summary</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#jdoom\">jDoom Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#jheretic\">jHeretic Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#jhexen\">jHexen Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#wolftc\">WolfTC Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#doom64tc\">Doom64TC Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#dehread\">dehread Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#mapload\">mapload Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#opengl\">OpenGL Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#d3d\">Direct3D Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#debugrenderer\">debugrenderer Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#openal\">OpenAL Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#sdlmixer\">sdlmixer Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#ds6\">DirectSound Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#a3d\">A3D Plugin</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "<li><a href=\"#common\">Plugin Common files</a></li>" >> $TOP_LEVEL_DIR/index.html
echo "</ul>" >> $TOP_LEVEL_DIR/index.html
}



echo "------------------------------------------------------------------------"
echo "The script will recursively audit all source files starting in the"
echo "current working directory. If it does not find it's magic strings it"
echo "will classify the code as 'Autogenerated or Unknown Licensed Code'."
echo "If you ARE NOT in the source directory you wish to audit"
echo "You WILL get incorrect results."
echo "------------------------------------------------------------------------"

if [[ -e $TOP_LEVEL_DIR/index.html ]]
then rm $TOP_LEVEL_DIR/index.html
fi
if [[ -e $TOP_LEVEL_DIR/code.html ]]
then rm $TOP_LEVEL_DIR/code.html
fi

echo "<html>" > $TOP_LEVEL_DIR/index.html
echo "<head>" >> $TOP_LEVEL_DIR/index.html
echo "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">" >> $TOP_LEVEL_DIR/index.html
echo "<title>The Doomsday Engine, Project Audit Summary</title>" >> $TOP_LEVEL_DIR/index.html
echo "<body bgcolor=\"#ffffff\" text=\"#000000\">" >> $TOP_LEVEL_DIR/index.html
echo "<a name=\"top\"> </a>" >> $TOP_LEVEL_DIR/index.html
echo "<h1>The Doomsday Engine, Project Audit Summary</h1>" >> $TOP_LEVEL_DIR/index.html
echo "<p> Last Updated `date` </p>" >> $TOP_LEVEL_DIR/index.html



## Check if in deng's top level directory. If so, audit all of deng,
## if not, then just the directory we are in
if [[ -e $TOP_LEVEL_DIR/doxygen ]]
then
	toc
	auditall
else
	findfiles
	scanfiles
	calcpercent
	consoleout
	htmlsummary
	htmllist
fi

rm $TOP_LEVEL_DIR/filelist.txt
rm $TOP_LEVEL_DIR/code.html
echo "</body>" >> $TOP_LEVEL_DIR/index.html
echo "</html>" >> $TOP_LEVEL_DIR/index.html
