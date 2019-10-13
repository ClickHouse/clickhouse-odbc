#!/usr/bin/env perl

use 5.16.0;
use utf8;
use open ':encoding(utf8)', ':std';
use strict;
use warnings;
use DBI;

eval {
    my $dbh = DBI->connect('dbi:ODBC:DSN=__nonexistent_dsn__', '', '', { RaiseError => 1, PrintError => 0 });
};

if ($@ =~ /\[([^\[\]]+)\]\[Driver Manager\]/) {
    print "$1\n";
}
elsif ($@) {
    die $@;
}
