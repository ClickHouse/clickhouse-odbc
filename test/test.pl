#!/usr/bin/env perl

# sudo apt install -y libdbd-odbc-perl
# sudo pkg install -y p5-dbd-odbc

use Test::More qw(no_plan);
use 5.16.0;
use utf8;
use open ':encoding(utf8)', ':std';
use DBI;
use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

my $config = {DSN => $ARGV[0] || $ENV{DSN} || 'clickhouse_localhost'};
my $is_wide = 1 if $config->{DSN} =~ /w$/;    # bad magic

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
$dbh->{odbc_utf8_on} = 1 if $is_wide;
say "odbc_has_unicode=$dbh->{odbc_has_unicode} is_wide=$is_wide";

sub prepare_execute_hash ($) {
    #warn "Executing: $_[0];";
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
    my ($n)     = @_;
    my $row     = prepare_execute_hash("SELECT '$n'")->[0];
    my ($value) = values %$row;
    my ($key)   = keys %$row;
    is $value, $n, "value eq " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
    is $key, qq{'$n'}, "header eq " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
}

sub test_one_string_value_as($;$) {
    my ($n, $expected) = @_;
    $expected //= $n;
    my $row = prepare_execute_hash("SELECT '$n' AS value")->[0];
    my ($value) = values %$row;
    is $value, $expected, "valueas eq " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
}

sub test_one_value_as($;$) {
    my ($n, $expected) = @_;
    $expected //= $n;
    my $row = prepare_execute_hash("SELECT $n AS value")->[0];
    my ($value) = values %$row;
    is $value, $expected, "valueas eq " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
}

#say Data::Dumper::Dumper prepare_execute_hash 'SELECT 1+1';
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options';
say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options ORDER BY length(name) ASC';
say Data::Dumper::Dumper prepare_execute_hash
q{SELECT *, (CASE WHEN (number == 1) THEN 'o' WHEN (number == 2) THEN 'two long string' WHEN (number == 3) THEN 'r' WHEN (number == 4) THEN NULL ELSE '-' END) FROM system.numbers LIMIT 6};
#TODO say Data::Dumper::Dumper prepare_execute_hash q{SELECT 1, 'string', NULL};
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT * FROM system.build_options ORDER BY length(name) DESC';
#say Data::Dumper::Dumper prepare_execute_hash q{SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'};

sub fn ($;@) { return '{fn ' . shift . "(" . (join ', ', @_) . ")}"; }
sub fn0 ($) { return fn(shift); }
sub fn1 ($;$) { return fn(shift, shift); }
sub fn2 ($$$) { return fn(shift, shift, shift); }

#say Data::Dumper::Dumper prepare_execute_hash 'SELECT {fn ABS({fn PI()})}';
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT '. fn('ABS', fn('PI') );
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' .  fn1 'ACOS', fn1 'ABS', fn1 'PI';

say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' . fn2 'POWER', 1,
  fn1 'ABS',
  fn1 'ACOS',
  fn1 'ASIN',
  fn1 'ATAN',
  fn1 'CEILING',
  fn1 'COS',
  fn1 'EXP',
  fn1 'FLOOR',
  fn1 'LOG',
  fn1 'LOG10',
  fn2 'MOD', 1,
  fn1 'RAND',
  fn1 'ROUND',
  fn1 'SIN',
  fn1 'SQRT',
  fn1 'TAN',
  fn1 'TRUNCATE',
  fn1 'PI',
  ;

#say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' . join ', ', (fn1 'HOUR', fn0 'NOW'), (fn1 'MINUTE', fn0 'NOW'),;
my $t = "toDateTime('2016-12-31 23:58:59')";
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' . join ', ', (fn1 'YEAR',       $t), (fn1 'MONTH',      $t), (fn1 'WEEK',       $t), (fn1 'HOUR',       $t), (fn1 'MINUTE', $t), (fn1 'SECOND', $t), (fn1 'DAYOFMONTH', $t), (fn1 'DAYOFWEEK',  $t), (fn1 'DAYOFYEAR',  $t),  ;
#say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' . join ', ',

test_one_value_as(fn('YEAR',       $t), 2016);
test_one_value_as(fn('MONTH',      $t), 12);
test_one_value_as(fn('DAYOFMONTH', $t), 31);
test_one_value_as(fn('HOUR',       $t), 23);
test_one_value_as(fn('MINUTE',     $t), 58);
test_one_value_as(fn('SECOND',     $t), 59);
#test_one_value_as(fn('WEEK',       $t),);
test_one_value_as(fn('DAYOFWEEK', $t), 7);
test_one_value_as(fn('DAYOFYEAR', $t), 366);

#say Data::Dumper::Dumper prepare_execute_hash 'SELECT ' . join ', ', (fn2 'IFNULL', 1, 2), (fn2 'IFNULL', 'NULL', 3);
test_one_value_as(fn('IFNULL', 1,      2), 1);
test_one_value_as(fn('IFNULL', 'NULL', 3), 3);

test_one_value_as(fn('CHAR_LENGTH',      "'abc'"),                      3);
test_one_value_as(fn('OCTET_LENGTH',     "'abc'"),                      3);
test_one_value_as(fn('LENGTH',     "'abc'"),                      3);
test_one_value_as(fn('CHAR_LENGTH',      "'йцукенгшщзхъ'"), 12);
test_one_value_as(fn('OCTET_LENGTH',     "'йцукенгшщзхъ'"), 24);
test_one_value_as(fn('LENGTH',     "'йцукенгшщзхъ'"), 12);
test_one_value_as(fn('CHARACTER_LENGTH', "'abc'"),                      3);
test_one_value_as(fn('CONCAT', "'abc'", "'123'"), 'abc123');
test_one_value_as(fn('LCASE', "'abcDEFghj'"), 'abcdefghj');
test_one_value_as(fn('UCASE', "'abcDEFghj'"), 'ABCDEFGHJ');
if ($is_wide) {
    test_one_value_as(fn('LCASE', "'йцуКЕН'"), 'йцукен');
    test_one_value_as(fn('UCASE', "'йцуКЕН'"), 'ЙЦУКЕН');
}
test_one_value_as(fn('REPLACE', "'abc'", "'b'", "'e'"), 'aec');
test_one_value_as(fn('SUBSTRING', "'abcd'", 2, 2), 'bc');

test_one_value_as(q{1+1}, 2);

if ($is_wide) {
    test_one_string_value(
q{абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ}
    );

    test_one_string_value_as(
q{абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ}
    );
}

#say Data::Dumper::Dumper prepare_execute_hash 'SELECT 1, sleep(25), sleep(15), 2'; # Default timeout is 30. Maximum allowed clickhouse sleep is 30s. We want to test 30+s

{
    use bigint;

    sub test_number($) {
        my ($n) = @_;
        my $row = prepare_execute_hash("SELECT $n")->[0];
        is $row->{$n}, $n, "n " . $n . " " . Data::Dumper->new([$row])->Indent(0)->Terse(1)->Sortkeys(1)->Dump();
    }

    for my $p (8, 16, 32, 64) {    # , 128
        my @res;
        my @range = $p == 64 ? (-1 .. -1) : (-1 .. 1);
        push @res, -1 * ($_ + 2**($p - 1)) for @range;    # -129 .. -127
        push @res, -1 + ($_ + 2**($p - 1)) for @range;    # 126 .. 128
        push @res, $_ + 2**$p for @range;                 # 255 .. 257
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
