#! /bin/sh
exec perl -w -x $0 ${1+"$@"}
#!perl -w
#line 5

# Purpose: A script to generate a dll.h file from the library name
# in a consistent way.

#
# fsm
#

my $program = "vxl_make_dll_h.pl";
my $library = $ARGV[0];

#
die "usage : $program <library>\n" unless defined($library) && !($library eq "");

# only allow alpha-numeric characters.
# should convert '-' to '_' ?
die "$program : character \'$1\' is not allowed\n" if ($library =~ m/([^a-zA-Z_0-9])/);

# upper-casify :
$LIBRARY = $library;
$LIBRARY =~ tr/a-z/A-Z/;

# if win32 and not buiding the DLL then you need a dllimport
# Only if you are building a DLL linked application.
print "#ifndef ${library}_dll_h_\n";
print "#define ${library}_dll_h_\n";
print "\n";
print "// this is a generated file. it was generated by\n";
print "// vxl/bin/$program with argument \'${library}\'\n";
print "\n";
print "#define ${LIBRARY}_DLL_DATA\n";
print "\n";
print "#if defined(WIN32) && !defined(BUILDING_${LIBRARY}_DLL)\n";
print "# ifdef BUILD_DLL\n";
print "#  undef ${LIBRARY}_DLL_DATA\n";
print "#  define ${LIBRARY}_DLL_DATA _declspec(dllimport)\n";
print "# endif // BUILD_DLL\n";
print "#endif // WIN32 and !BUILDING_${LIBRARY}_DLL\n";
print "\n";
print "#endif\n";