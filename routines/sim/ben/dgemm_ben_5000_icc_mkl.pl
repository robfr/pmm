#!/usr/bin/env perl 
#
#   Copyright (C) 2008-2010 Robert Higgins
#       Author: Robert Higgins <robert.higgins@ucd.ie>
#
#   This file is part of PMM.
#
#   PMM is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   PMM is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with PMM.  If not, see <http://www.gnu.org/licenses/>.
#

#
# This script reads recorded performance data on matrix multiplication and
# returns performance at requested problem size.
#
# The matrix multiplication is C=A*B where A is a square MxK matrix size
# 4096x4096 and B is a KxN matrix of size Kx4096. K is the sole input parameter
# to this program
#

use strict;
use POSIX qw/ceil/,qw/floor/;
use Text::ParseWords('quotewords');
use FindBin qw($Bin);


# check argument was passed
if($#ARGV != 1) {
    print "Argument parsing error.\n";
    exit -1;
}

my $datafile = "$Bin/hoja_dgemm_5000_icc_mkl.csv";

my $omp_thrds = $ARGV[0];
my $lib_thrds = $ARGV[1];

my $size = 5000;
my $nflops = 2*$size**3;
my $cputime = -1;

my $cc = "icc";
my $lib = "mkl";

# indexes for problem size and execution time in the CSV data
my $size_i;
my $time_i;
my $omp_thrds_i;
my $lib_thrds_i;

# calculate the first index based on the the compiler and library being used
if($cc eq "icc" && $lib eq "mkl") {
    $size_i = 5;
}
elsif($cc eq "gcc" && $lib eq "mkl") {
    $size_i = 10;
}
elsif($cc eq "icc" && $lib eq "atl") {
    $size_i = 15;
}
elsif($cc eq "gcc" && $lib eq "atl") {
    $size_i = 20;
}
else {
    print "Compiler and library not matched ($cc:$lib)\n";
    return -5;
}

# all other indexes are offset from the first
$omp_thrds_i = $size_i+1;
$lib_thrds_i = $size_i+2;
$time_i = $size_i+3;

open(DATA, "<$datafile") or die "Could not read file: $datafile";

<DATA>; # read the first line of data and discard it (it is the column labels)

my @line;
while(<DATA>) {

    @line = quotewords(",",0,$_); # parse comma seperated line into array

    if($line[$size_i] == $size &&
       $line[$omp_thrds_i] == $omp_thrds &&
       $line[$lib_thrds_i] == $lib_thrds)
    {
        $cputime = $line[$time_i];
        last;
    }

    #printf "$line[$size_i]  $line[$omp_thrds_i] $line[$lib_thrds_i] $line[$time_i]\n";

}

if($nflops != -1 && $cputime >= 0) {

    print_sec_usec($cputime); # pmm expects wall time and user+kernel time so
    print_sec_usec($cputime); # we give it the same for both
    print $nflops . "\n";
}
else {
    print "Error finding benchmark in file ($omp_thrds,$lib_thrds)\n";
    exit -2;
}

exit 0;

sub print_sec_usec {
    my $sec = floor($_[0]);
    my $usec = floor(($_[0] - $sec)*1000000);
    print $sec . " " . $usec . "\n";
}
