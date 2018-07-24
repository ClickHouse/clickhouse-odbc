#!/usr/bin/env perl

# sudo apt install -y libdbd-odbc-perl
# sudo pkg install -y p5-dbd-odbc

use 5.16.0;
use DBI;
use Data::Dumper;

my $config = {DSN => $ENV{DSN} || 'clickhouse_localhost'};

say 'Data sources: ', join '; ', DBI->data_sources('dbi:ODBC:DSN=' . $config->{DSN},);

my $dbh = DBI->connect(
    'dbi:ODBC:DSN=' . $config->{DSN},
    $config->{'user'},
    $config->{'password'}, {
        RaiseError => 1,
        PrintError => 1,
        #HandleError=>sub{my ($msg) = @_; warn "connect error:", $msg; return 0;     },
    }
);

sub prepare_execute_hash ($) {
    my $sth = $dbh->prepare($_[0]);
    #$sth->{LongReadLen} = 1024 * 1024;
    $sth->{LongReadLen} = 255 * 255;
    $sth->{LongTruncOk} = 1;
    return undef unless $sth->execute();
    my $ret;
    while (my $hash_ref = $sth->fetchrow_hashref()) {
        push @$ret, $hash_ref;
    }
    return $ret;
}

say Data::Dumper::Dumper prepare_execute_hash 'SELECT 1+1';

say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options';
say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options ORDER BY length(name) ASC';
say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options ORDER BY length(name) DESC';
