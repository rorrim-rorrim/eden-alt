#!/usr/bin/perl
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
use strict;
use warnings;
use List::Util qw(uniq);
open STRINGS, '<', 'src/android/app/src/main/res/values/strings.xml';
my @raw_strings = <STRINGS>;
my @line_strings = grep {/\bstring name="\b/} @raw_strings;
my @strings = map { my @x = split('"', $_); $x[1] } @line_strings;
close STRINGS;
my $pattern = join('|', @strings);
my $regex = qr/($pattern)/;
my @unused_strings = @strings;
sub process_file {
    my $b = $_[1];
    open FILE, "<".$_[0] or die "cant open ".$_[0];
    #print "<!-- FILE: ".$_[0]." -->"."\n";
    while (my $line = <FILE>) {
        $line =~ s/^\s+|\s+$//g; #trim
        if ($line =~ $regex) {
            foreach my $e (@strings) {
                my $re = $b != 0 ? ('\b'.quotemeta("R.string.".$e).'\b') : quotemeta("\@string/".$e);
                #print "<!--             match: ".$e." -> ".$line."-->\n" if $line =~ /$re/;
                @unused_strings = grep {!/$e/} @unused_strings if $line =~ /$re/;
            }
        }
    }
    close FILE;
}
# xml files
open XML_LIST, "find src/android/app/src/main -type f -iname '*.xml' |" || die;
foreach (<XML_LIST>) { process_file($_, 0) if $_ !~ /\/strings.xml/; }
close XML_LIST;
# kotlin
open KOT_LIST, "find src/android/app/src/main -type f -iname '*.kt' |" || die;
foreach (<KOT_LIST>) { process_file $_, 1; }
close KOT_LIST;
#print "unused strings: ".scalar(@unused_strings)."\n";
#foreach (@unused_strings) { print $_."\n"; }
open STRINGS, '<', 'src/android/app/src/main/res/values/strings.xml';
while (my $line = <STRINGS>) {
    my $b = 1;
    if ($line =~ $regex) {
        foreach my $e (@unused_strings) {
            my $re = quotemeta('"'.$e.'"');
            print "<!-- UNUSED ".$line." : ".$e."-->\n" if $line =~ /$re/;
            $b = $line =~ /$re/ ? 0 : $b;
        }
    }
    print $line if $b > 0;
}
close STRINGS;
