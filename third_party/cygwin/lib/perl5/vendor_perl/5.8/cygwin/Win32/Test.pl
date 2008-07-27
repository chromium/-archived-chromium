#####
#       T E S T . P L
#       -------------
#       A test script for exercising the Win32::ODBC extension. Install
#       the ODBC.PLL extension and the ODBC.PM wrapper, set up an ODBC
#       DSN (Data Source Name) by the ODBC administrator, then give this a try!
#
#       READ THE DOCUMENTATION -- I AM NOT RESPOSIBLE FOR ANY PROBLEMS THAT
#       THIS MAY CAUSE WHATSOEVER.  BY USING THIS OR ANY  ---
#       OF THE WIN32::ODBC PARTS FOUND IN THE DISTRIBUTION YOU ARE AGREEING
#       WITH THE TERMS OF THIS DISTRIBUTION!!!!!
#
#
#       You have been warned.
#       --- ---- ---- ------
#
#       Updated to test newest version v961007.  Dave Roth <rothd@roth.net>
#           This version contains a small sample database (in MS Access 7.0
#           format) called ODBCTest.mdb. Place this database in the same
#           directory as this test.pl file.
#
#       --------------------------------------------------------------------
#
#       SYNTAX:
#           perl test.pl ["DSN Name" [Max Rows]]
#
#               DSN Name....Name of DSN that will be used. If this is
#                           omitted then we will use the obnoxious default DSN.
#               Max Rows....Maximum number of rows wanted to be retrieved from
#                           the DSN.
#                           - If this is 0 then the request is to retrieve as
#                             many as possible.
#                           - If this is a value greater than that which the DSN
#                             driver can handle the value will be the greatest
#                             the DSN driver allows.
#                           - If omitted then we use the default value.
#####

    use Win32::ODBC;


    $TempDSN = "Win32 ODBC Test --123xxYYzz987--";
    $iTempDSN = 1;

    if (!($DSN = $ARGV[0])){
        $DSN = $TempDSN;
    }
    $MaxRows = 8 unless defined ($MaxRows = $ARGV[1]);

    $DriverType = "Microsoft Access Driver (*.mdb)";
    $Desc = "Description=The Win32::ODBC Test DSN for Perl";
    $Dir = Win32::GetCwd();
    $DBase = "ODBCTest.mdb";

    $iWidth=60;
    %SQLStmtTypes = (SQL_CLOSE, "SQL_CLOSE", SQL_DROP, "SQL_DROP", SQL_UNBIND, "SQL_UNBIND", SQL_RESET_PARAMS, "SQL_RESET_PARAMS");

#    Initialize();

    ($Name, $Version, $Date, $Author, $CompileDate, $CompileTime, $Credits) = Win32::ODBC::Info();
    print "\n";
    print "\t+", "=" x ($iWidth), "+\n";
    print "\t|", Center("", $iWidth), "|\n";
    print "\t|", Center("", $iWidth), "|\n";
    print "\t|", Center("$Name", $iWidth), "|\n";
    print "\t|", Center("-" x length("$Name"), $iWidth), "|\n";
    print "\t|", Center("", $iWidth), "|\n";

    print "\t|", Center("Version $Version ($Date)", $iWidth), "|\n";
    print "\t|", Center("by $Author", $iWidth), "|\n";
    print "\t|", Center("Compiled on $CompileDate at $CompileTime.", $iWidth), "|\n";
    print "\t|", Center("", $iWidth), "|\n";
    print "\t|", Center("Credits:", $iWidth), "|\n";
    print "\t|", Center(("-" x length("Credits:")), $iWidth), "|\n";
    foreach $Temp (split("\n", $Credits)){
        print "\t|", Center("$Temp", $iWidth), "|\n";
    }
    print "\t|", Center("", $iWidth), "|\n";
    print "\t+", "=" x ($iWidth), "+\n";

####
#   T E S T  1
####
    PrintTest(1, "Dump available ODBC Drivers");
    print "\nAvailable ODBC Drivers:\n";
    if (!(%Drivers = Win32::ODBC::Drivers())){
        $Failed{'Test 1'} = "Drivers(): " . Win32::ODBC::Error();
    }
    foreach $Driver (keys(%Drivers)){
        print "  Driver=\"$Driver\"\n  Attributes: ", join("\n" . " "x14, sort(split(';', $Drivers{$Driver}))), "\n\n";
    }


####
#   T E S T  2
####
    PrintTest(2,"Dump available datasources");

    ####
    #   Notice you don't need an instantiated object to use this...
    ####
    print "\nHere are the available datasources...\n";
    if (!(%DSNs = Win32::ODBC::DataSources())){
        $Failed{'Test 2'} = "DataSources(): " . Win32::ODBC::Error();
    }
    foreach $Temp (keys(%DSNs)){
        if (($Temp eq $TempDSN) && ($DSNs{$Temp} eq $DriverType)){
            $iTempDSNExists++;
        }
        if ($DSN =~ /$Temp/i){
            $iTempDSN = 0;
            $DriverType = $DSNs{$Temp};
        }
        print "\tDSN=\"$Temp\" (\"$DSNs{$Temp}\")\n";
    }

####
#   T E S T 2.5
####
        #   IF WE DO NOT find the DSN the user specified...
    if ($iTempDSN){
        PrintTest("2.5", "Create a Temporary DSN");

        print "\n\tCould not find the DSN (\"$DSN\") so we will\n\tuse a temporary one (\"$TempDSN\")...\n\n";

        $DSN = $TempDSN;

        if (! $iTempDSNExists){
            print "\tAdding DSN \"$DSN\"...";
            if (Win32::ODBC::ConfigDSN(ODBC_ADD_DSN, $DriverType, ("DSN=$DSN", "Description=The Win32 ODBC Test DSN for Perl", "DBQ=$Dir\\$DBase", "DEFAULTDIR=$Dir", "UID=", "PWD="))){
                print "Successful!\n";
            }else{
                print "Failure\n";
                $Failed{'Test 2.5'} = "ConfigDSN(): Could not add \"$DSN\": " . Win32::ODBC::Error();
                    # If we failed here then use the last DSN we listed in Test 2
                $DriverType = $DSNs{$Temp};
                $DSN = $Temp;
                print "\n\tCould not add a temporary DSN so using the last one listed:\n";
                print "\t\t$DSN ($DriverType)\n";

            }
        }
    }

####
#   Report What Driver/DSN we are using
####

    print "\n\nWe are using the DSN:\n\tDSN = \"$DSN\"\n";
    print "\tDriver = \"$DriverType\"\n\n";


####
#   T E S T  3
####
    PrintTest(3, "Open several ODBC connections");
    print "\n\tOpening ODBC connection for \"$DSN\"...\n\t\t";
    if (!($O = new Win32::ODBC($DSN))){
        print "Failure. \n\n";
        $Failed{'Test 3a'} = "new(): " . Win32::ODBC::Error();
        PresentErrors();
        exit();
    }else{
        print "Success (connection #", $O->Connection(), ")\n\n";
    }

    print "\tOpening ODBC connection for \"$DSN\"...\n\t\t";
    if (!($O2 = new Win32::ODBC($DSN))){
        $Failed{'Test 3b'} = "new(): " . Win32::ODBC::Error();
        print "Failure. \n\n";
    }else{
        print "Success (connection #", $O2->Connection(), ")\n\n";
    }

    print "\tOpening ODBC connection for \"$DSN\"\n\t\t";
    if (!($O3 = new Win32::ODBC($DSN))){
        $Failed{'Test 3c'} = "new(): " . Win32::ODBC::Error();
        print "Failure. \n\n";
    }else{
        print "Success (connection #", $O3->Connection(), ")\n\n";
    }


####
#   T E S T  4
####
    PrintTest(4, "Close all but one connection");

    print "\n\tCurrently open ODBC connections are: \"", join(", ", sort($O2->GetConnections())), "\"\n";
    print "\tClosing ODBC connection #", $O2->Connection(), "...\n";
    print "\t\t...", (($O2->Close())? "Successful.":"Failure."), "\n";

    print "\n\tCurrently open ODBC connections are: \"", join(", ", sort($O3->GetConnections())), "\"\n";
    print "\tClosing ODBC connection #", $O3->Connection(), "...\n";
    print "\t\t...", (($O3->Close())? "Successful.":"Failure."), "\n";

    print "\n\tCurrently open ODBC connections are: \"", join(", ", sort($O2->GetConnections())), "\"\n";

####
#   T E S T  5
####
    PrintTest(5, "Set/query Max Buffer size for a connection");

    srand(time);
    $Temp = int(rand(10240)) + 10240;
    print "\nMaximum Buffer Size for connection #", $O->Connection(), ":\n";
    print "\tValue set at ", $O->GetMaxBufSize(), "\n";

    print "\tSetting Maximum Buffer Size to $Temp...  it has been set to ", $O->SetMaxBufSize($Temp), "\n";
    print "\tValue set at ", $O->GetMaxBufSize(), "\n";

    $Temp += int(rand(10240)) + 102400;
    print "\tSetting Maximum Buffer Size to $Temp... (can not be more than 102400)\n\t\t...it has been set to ", $O->SetMaxBufSize($Temp), "\n";
    print "\tValue set at ", $O->GetMaxBufSize(), "\n";

    $Temp = int(rand(1024)) + 2048;
    print "\tSetting Maximum Buffer Size to $Temp...  it has been set to ", $O->SetMaxBufSize($Temp), "\n";

    print "\tValue set at ", $O->GetMaxBufSize(), "\n";


####
#   T E S T  6
####
    PrintTest(6, "Set/query Stmt Close Type");

    print "\n\tStatement Close Type is currently set as ", $O->GetStmtCloseType(), " " . $O->Error . "\n";
    print "\tSetting Statement Close Type to SQL_CLOSE: (returned code of ",  $O->SetStmtCloseType(SQL_CLOSE), ")" . $O->Error . "\n";
    print "\tStatement Close Type is currently set as ", $O->GetStmtCloseType(), " " . $O->Error ."\n";


####
#   T E S T  7
####
    PrintTest(7, "Dump DSN for current connection");

    if (! (%DSNAttributes = $O->GetDSN())){
        $Failed{'Test 7'} = "GetDSN(): " . $O->Error();
    }else{
        print"\nThe DSN for connection #", $O->Connection(), ":\n";
        print "\tDSN...\n";
        foreach (sort(keys(%DSNAttributes))){
            print "\t$_ = \"$DSNAttributes{$_}\"\n";
        }
    }



####
#   T E S T  8
####
    PrintTest(8, "Dump list of ALL tables in datasource");

    print "\nList of tables for \"$DSN\"\n\n";
    $Num = 0;
    if ($O->Catalog("", "", "%", "'TABLE','VIEW','SYSTEM TABLE', 'GLOBAL TEMPORARY','LOCAL TEMPORARY','ALIAS','SYNONYM'")){

        print "\tCursor is currently named \"", $O->GetCursorName(), "\".\n";
        print "\tRenaming cursor to \"TestCursor\"...", (($O->SetCursorName("TestCursor"))? "Success":"Failure"), ".\n";
        print "\tCursor is currently named \"", $O->GetCursorName(), "\".\n\n";

        @FieldNames = $O->FieldNames();

        $~ = "Test_8_Header";
        write;

        $~ = "Test_8_Body";
        while($O->FetchRow()){
            undef %Data;
            %Data = $O->DataHash();
            write;
        }
    }
    print "\n\tTotal number of tables displayed: $Num\n";



####
#   T E S T  9
####
    PrintTest(9, "Dump list of non-system tables and views in datasource");

    print "\n";
    $Num = 0;

    foreach  $Temp ($O->TableList("", "", "%", "TABLE, VIEW, SYSTEM_TABLE")){
        $Table = $Temp;
        print "\t", ++$Num, ".) \"$Temp\"\n";
    }
    print "\n\tTotal number of tables displayed: $Num\n";


####
#   T E S T  10
####
    PrintTest(10, "Dump contents of the table: \"$Table\"");

    print "\n";

    print "\tResetting (dropping) cursor...", (($O->DropCursor())? "Successful":"Failure"), ".\n\n";

    print "\tCurrently the cursor type is: ", $O->GetStmtOption($O->SQL_CURSOR_TYPE), "\n";
    print "\tSetting Cursor to Dynamic (", ($O->SQL_CURSOR_DYNAMIC), ")...", (($O->SetStmtOption($O->SQL_CURSOR_TYPE, $O->SQL_CURSOR_DYNAMIC))? "Success":"Failure"), ".\n";
    print "\t\tThis may have failed depending on your ODBC Driver.\n";
    print "\t\tThis is not really a problem, it will default to another value.\n";
    print "\tUsing the cursor type of: ", $O->GetStmtOption($O->SQL_CURSOR_TYPE), "\n\n";

    print "\tSetting the connection to only grab $MaxRows row", ($MaxRows == 1)? "":"s", " maximum...";
    if ($O->SetStmtOption($O->SQL_MAX_ROWS, $MaxRows)){
        print "Success!\n";
    }else{
        $Failed{'Test 10a'} = "SetStmtOption(): " . Win32::ODBC::Error();
        print "Failure.\n";
    }

    $iTemp = $O->GetStmtOption($O->SQL_MAX_ROWS);
    print "\tUsing the maximum rows: ", (($iTemp)? $iTemp:"No maximum limit"), "\n\n";

    print "\tCursor is currently named \"", $O->GetCursorName(), "\".\n";
    print "\tRenaming cursor to \"TestCursor\"...", (($O->SetCursorName("TestCursor"))? "Success":"Failure"), ".\n";
    print "\tCursor is currently named \"", $O->GetCursorName(), "\".\n\n";

    if (! $O->Sql("SELECT * FROM [$Table]")){
        @FieldNames = $O->FieldNames();
        $Cols = $#FieldNames + 1;
        $Cols = 8 if ($Cols > 8);

        $FmH = "format Test_10_Header =\n";
        $FmH2 = "";
        $FmH3 = "";
        $FmB = "format Test_10_Body = \n";
        $FmB2 = "";

        for ($iTemp = 0; $iTemp < $Cols; $iTemp++){
            $FmH .= "@" . "<" x (80/$Cols - 2) . " ";
            $FmH2 .= "\$FieldNames[$iTemp],";
            $FmH3 .= "-" x (80/$Cols - 1) . " ";

            $FmB .= "@" . "<" x (80/$Cols - 2) . " ";
            $FmB2 .= "\$Data{\$FieldNames[$iTemp]},";
        }
        chop $FmH2;
        chop $FmB2;

        eval"$FmH\n$FmH2\n$FmH3\n.\n";
        eval "$FmB\n$FmB2\n.\n";

        $~ = "Test_10_Header";
        write();
        $~ = "Test_10_Body";

            # Fetch the next rowset
        while($O->FetchRow()){
            undef %Data;
            %Data = $O->DataHash();
            write();
        }
            ####
            #   THE preceeding block could have been written like this:
            #   ------------------------------------------------------
            #
            #       print "\tCurrently the cursor type is: ", $O->GetStmtOption($O->SQL_CURSOR_TYPE), "\n";
            #       print "\tSetting Cursor to Dynamic (", ($O->SQL_CURSOR_DYNAMIC), ")...", (($O->SetStmtOption($O->SQL_CURSOR_TYPE, $O->SQL_CURSOR_DYNAMIC))? "Success":"Failure"), ".\n";
            #       print "\t\tThis may have failed depending on your ODBC Driver. No real problem.\n";
            #       print "\tUsing the cursor type of: ", $O->GetStmtOption($O->SQL_CURSOR_TYPE), "\n\n";
            #
            #       print "\tSetting rowset size = 15 ...", (($O->SetStmtOption($O->SQL_ROWSET_SIZE, 15))? "Success":"Failure"), ".\n";
            #       print "\tGetting rowset size: ", $O->GetStmtOption($O->SQL_ROWSET_SIZE), "\n\n";
            #
            #       while($O->FetchRow()){
            #           $iNum = 1;
            #               #  Position yourself in the rowset
            #           while($O->SetPos($iNum++ ,$O->SQL_POSITION, $O->SQL_LOCK_NO_CHANGE)){
            #               undef %Data;
            #               %Data = $O->DataHash();
            #               write();
            #           }
            #           print "\t\tNext rowset...\n";
            #       }
            #
            #   The reason I didn't write it that way (which is easier) is to
            #   show that we can now SetPos(). Also Fetch() now uses
            #   SQLExtendedFetch() so it can position itself and retrieve
            #   rowsets. Notice earlier in this Test 10 we set the
            #   SQL_ROWSET_SIZE. If this was not set it would default to
            #   no limit (depending upon your ODBC Driver).
            ####

        print "\n\tNo more records available.\n";
    }else{
        $Failed{'Test 10'} = "Sql(): " . $O->Error();
    }

    $O->Close();

####
#   T E S T 11
####
    if ($iTempDSN){
        PrintTest(11, "Remove the temporary DSN");
        print "\n\tRemoving the temporary DSN:\n";
        print "\t\tDSN = \"$DSN\"\n\t\tDriver = \"$DriverType\"\n";

        if (Win32::ODBC::ConfigDSN(ODBC_REMOVE_DSN, $DriverType, "DSN=$DSN")){
            print "\tSuccessful!\n";
        }else{
            print "\tFailed.\n";
            $Failed{'Test 11'} = "ConfigDSN(): Could not remove \"$DSN\":" . Win32::ODBC::Error();
        }
    }


    PrintTest("E N D   O F   T E S T");
    PresentErrors();



#----------------------- F U N C T I O N S ---------------------------

sub Error{
    my($Data) = @_;
    $Data->DumpError() if ref($Data);
    Win32::ODBC::DumpError() if ! ref($Data);
}


sub Center{
    local($Temp, $Width) = @_;
    local($Len) = ($Width - length($Temp)) / 2;
    return " " x int($Len), $Temp, " " x (int($Len) + (($Len != int($Len))? 1:0));
}

sub PrintTest{
    my($Num, $String) = @_;
    my($Temp);
    if (length($String)){
        $Temp = "  T E S T  $Num $String ";
    }else{
        $Temp = "  $Num  ";
    }
    $Len = length($Temp);
    print "\n", "-" x ((79 - $Len)/2), $Temp, "-" x ((79 - $Len)/2 - 1), "\n";
    print "\t$String\n";
}

sub PresentErrors{
    PrintTest("", "Error Report:");
    if (keys(%Failed)){
        print "The following were errors:\n";
        foreach (sort(keys(%Failed))){
            print "$_ = $Failed{$_}\n";
        }
    }else{
        print "\n\nThere were no errors reported during this test.\n\n";
    }
}


sub Initialize{
format Test_8_Header =
       @<<<<<<<<<<<<<<<<<<<<<<<<<<< @|||||||||||| @|||||||||||| @|||||||||||
       $FieldNames[0],     $FieldNames[1], $FieldNames[2], $FieldNames[3]
       ---------------------------- ------------- ------------- ------------
.
format Test_8_Body =
   @>. @<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<< @<<<<<<<<<<<< @<<<<<<<<<<<
 ++$Num, $Data{$FieldNames[0]},  $Data{$FieldNames[1]},   $Data{$FieldNames[2]}, $Data{$FieldNames[3]}
.
format Test_9_Header =
          @<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<< @<<<<<<<<<<<<<< @<<<<<<<<<<<<<<
           $FieldNames[0],  $FieldNames[1],   $FieldNames[2], $FieldNames[3]
.
format Test_9_Body =
          @<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<< @<<<<<<<<<<<<<< @<<<<<<<<<<<<<<
           $Data{$FieldNames[0]},  $Data{$FieldNames[1]},   $Data{$FieldNames[2]}, $Data{$FieldNames[3]}
.
}
