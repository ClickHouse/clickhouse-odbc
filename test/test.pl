#!/usr/bin/env perl

# sudo apt install -y libdbd-odbc-perl
# sudo pkg install -y p5-dbd-odbc

use Test::More qw(no_plan);
use 5.16.0;
use DBI;
use Data::Dumper;
$Data::Dumper::Sortkeys=1;

my $config = {DSN => $ARGV[0] || $ENV{DSN} || 'clickhouse_localhost'};

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

sub test_one_string_value($) {
    my ($n) = @_;
    my $row = prepare_execute_hash("SELECT '$n'")->[0];
    my ($value) = values %$row;
    my ($key) = keys %$row;
    is $value, $n, "value eq " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
    is $key, qq{'$n'}, "header eq " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
}


say Data::Dumper::Dumper prepare_execute_hash 'SELECT 1+1';
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options';
say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options ORDER BY length(name) ASC';
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options ORDER BY length(name) DESC';
#say Data::Dumper::Dumper prepare_execute_hash q{SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'};
test_one_string_value(q{абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ});

say Data::Dumper::Dumper prepare_execute_hash q{SELECT *, (CASE WHEN (number == 1) THEN 'o' WHEN (number == 2) THEN 'two long string' WHEN (number == 3) THEN 'r' ELSE '-' END)  FROM system.numbers LIMIT 5};

#say Data::Dumper::Dumper prepare_execute_hash 'SELECT 1, sleep(25), sleep(15), 2'; # Default timeout is 30. Maximum allowed clickhouse sleep is 30s. We want to test 30+s

{
use bigint;
sub test_number($) {
    my ($n) = @_;
    my $row = prepare_execute_hash("SELECT $n")->[0];
    is $row->{$n}, $n, "n " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
}

for my $p (8, 16, 32, 64) { # , 128
    my @res;
    my @range = $p == 64 ? (-1 .. -1) : (-1..1);
    push @res, -1*($_+2**($p-1)) for @range; # -129 .. -127
    push @res, -1+($_+2**($p-1)) for @range; # 126 .. 128
    push @res, $_+2**$p for @range;          # 255 .. 257
    #say join ',', @res;
    #say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' . join ',', @res;
    test_number $_ for @res;
}

    say Data::Dumper::Dumper prepare_execute_hash 'SELECT 2147483647, 2147483648, 2147483647+1, 2147483647+10, 4294967295';

# -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551616,18446744073709551617
# say Data::Dumper::Dumper prepare_execute_hash 'SELECT -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551616,18446744073709551617';
# without overflow
# say Data::Dumper::Dumper prepare_execute_hash 'SELECT -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615';
}

done_testing();
