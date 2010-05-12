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

my $datafile = "$Bin/dgemm_4096_gpu_cpu_data.csv";
my $pu = $ARGV[0];

if($pu != "cpu" || $pu != "gpu") {
    print "Argument parsing error.\n";
}

my $k = $ARGV[1];

my $k_nflops = -1;
my $k_cputime = -1;
my $k_gputime = -1;

open(DATA, "<$datafile") or die "Could not read file: $datafile";

<DATA>; # read the first line of data and discard it (it is the column labels)

my @line;
while(<DATA>) {

    @line = quotewords(",",0,$_); # parse comma seperated line into array

    if($line[1] == $k) {
        $k_nflops = $line[3];
        $k_gputime = $line[5];
        $k_cputime = $line[6];
        last;
    }

    #my $element;
    #foreach $element (@line) {
    #    print $element . " ";
    #}
    #print "\n";
}

if($k_nflops != -1) {

    if($pu == "cpu") {
        print_sec_usec($k_cputime); # pmm expects wall time and user+kernel time so
        print_sec_usec($k_cputime); # we give it the same for both
    }
    elsif($pu == "gpu") {
        print_sec_usec($k_gputime); # pmm expects wall time and user+kernel time so
        print_sec_usec($k_gputime); # we give it the same for both
    }

    print $k_nflops . "\n";
}
else {
    exit -2;
}

exit 0;

sub print_sec_usec {
    my $sec = floor($_[0]);
    my $usec = floor(($_[0] - $sec)*1000000);
    print $sec . " " . $usec . "\n";
}
