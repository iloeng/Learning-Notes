# TODO
# test pipelining

set ::passed 0
set ::failed 0

proc test {name code okpattern} {
    puts -nonewline [format "%-70s " $name]
    flush stdout
    set retval [uplevel 1 $code]
    if {$okpattern eq $retval || [string match $okpattern $retval]} {
        puts "PASSED"
        incr ::passed
    } else {
        puts "!! ERROR expected '$okpattern' but got '$retval'"
        incr ::failed
    }
}

proc main {server port} {
    set fd [redis_connect $server $port]

    test {DEL all keys to start with a clean DB} {
        foreach key [redis_keys $fd *] {
            redis_del $fd $key
        }
        redis_dbsize $fd
    } {0}

    test {SET and GET an item} {
        redis_set $fd x foobar
        redis_get $fd x
    } {foobar}

    test {DEL against a single item} {
        redis_del $fd x
        redis_get $fd x
    } {}

    test {KEYS with pattern} {
        foreach key {key_x key_y key_z foo_a foo_b foo_c} {
            redis_set $fd $key hello
        }
        lsort [redis_keys $fd foo*]
    } {foo_a foo_b foo_c}

    test {KEYS to get all keys} {
        lsort [redis_keys $fd *]
    } {foo_a foo_b foo_c key_x key_y key_z}

    test {DBSIZE} {
        redis_dbsize $fd
    } {6}

    test {DEL all keys} {
        foreach key [redis_keys $fd *] {
            redis_del $fd $key
        }
        redis_dbsize $fd
    } {0}

    test {Very big payload in GET/SET} {
        set buf [string repeat "abcd" 1000000]
        redis_set $fd foo $buf
        redis_get $fd foo
    } [string repeat "abcd" 1000000]

    test {SET 10000 numeric keys and access all them in reverse order} {
        for {set x 0} {$x < 10000} {incr x} {
            redis_set $fd $x $x
        }
        set sum 0
        for {set x 9999} {$x >= 0} {incr x -1} {
            incr sum [redis_get $fd $x]
        }
        format $sum
    } {49995000}

    test {DBSIZE should be 10001 now} {
        redis_dbsize $fd
    } {10001}

    test {INCR against non existing key} {
        set res {}
        append res [redis_incr $fd novar]
        append res [redis_get $fd novar]
    } {11}

    test {INCR against key created by incr itself} {
        redis_incr $fd novar
    } {2}

    test {INCR against key originally set with SET} {
        redis_set $fd novar 100
        redis_incr $fd novar
    } {101}

    test {SETNX target key missing} {
        redis_setnx $fd novar2 foobared
        redis_get $fd novar2
    } {foobared}

    test {SETNX target key exists} {
        redis_setnx $fd novar2 blabla
        redis_get $fd novar2
    } {foobared}

    test {EXISTS} {
        set res {}
        redis_set $fd newkey test
        append res [redis_exists $fd newkey]
        redis_del $fd newkey
        append res [redis_exists $fd newkey]
    } {10}

    test {Zero length value in key. SET/GET/EXISTS} {
        redis_set $fd emptykey {}
        set res [redis_get $fd emptykey]
        append res [redis_exists $fd emptykey]
        redis_del $fd emptykey
        append res [redis_exists $fd emptykey]
    } {10}

    test {Commands pipelining} {
        puts -nonewline $fd "SET k1 4\r\nxyzk\r\nGET k1\r\nPING\r\n"
        flush $fd
        set res {}
        append res [string match +OK* [redis_read_retcode $fd]]
        append res [redis_bulk_read $fd]
        append res [string match +PONG* [redis_read_retcode $fd]]
        format $res
    } {1xyzk1}

    test {Non existing command} {
        puts -nonewline $fd "foo\r\n"
        flush $fd
        string match -ERR* [redis_read_retcode $fd]
    } {1}

    test {Basic LPUSH, RPUSH, LLENGTH, LINDEX} {
        redis_lpush $fd mylist a
        redis_lpush $fd mylist b
        redis_rpush $fd mylist c
        set res [redis_llen $fd mylist]
        append res [redis_lindex $fd mylist 0]
        append res [redis_lindex $fd mylist 1]
        append res [redis_lindex $fd mylist 2]
    } {3bac}

    test {DEL a list} {
        redis_del $fd mylist
        redis_exists $fd mylist
    } {0}

    test {Create a long list and check every single element with LINDEX} {
        set ok 0
        for {set i 0} {$i < 1000} {incr i} {
            redis_rpush $fd mylist $i
        }
        for {set i 0} {$i < 1000} {incr i} {
            if {[redis_lindex $fd mylist $i] eq $i} {incr ok}
            if {[redis_lindex $fd mylist [expr (-$i)-1]] eq [expr 999-$i]} {
                incr ok
            }
        }
        format $ok
    } {2000}

    test {Test elements with LINDEX in random access} {
        set ok 0
        for {set i 0} {$i < 1000} {incr i} {
            set r [expr int(rand()*1000)]
            if {[redis_lindex $fd mylist $r] eq $r} {incr ok}
            if {[redis_lindex $fd mylist [expr (-$r)-1]] eq [expr 999-$r]} {
                incr ok
            }
        }
        format $ok
    } {2000}

    test {LLEN against non-list value error} {
        redis_del $fd mylist
        redis_set $fd mylist foobar
        redis_llen $fd mylist
    } {-1}

    test {LINDEX against non-list value error} {
        redis_lindex $fd mylist 0
    } {*ERROR*}

    test {LPUSH against non-list value error} {
        redis_lpush $fd mylist 0
    } {-ERR*}

    test {RPUSH against non-list value error} {
        redis_rpush $fd mylist 0
    } {-ERR*}

    test {RENAME basic usage} {
        redis_set $fd mykey hello
        redis_rename $fd mykey mykey1
        redis_rename $fd mykey1 mykey2
        redis_get $fd mykey2
    } {hello}

    test {RENAME source key should no longer exist} {
        redis_exists $fd mykey
    } {0}

    test {RENAME against already existing key} {
        redis_set $fd mykey a
        redis_set $fd mykey2 b
        redis_rename $fd mykey2 mykey
        set res [redis_get $fd mykey]
        append res [redis_exists $fd mykey2]
    } {b0}

    test {RENAMENX basic usage} {
        redis_del $fd mykey
        redis_del $fd mykey2
        redis_set $fd mykey foobar
        redis_renamenx $fd mykey mykey2
        set res [redis_get $fd mykey2]
        append res [redis_exists $fd mykey]
    } {foobar0}

    test {RENAMENX against already existing key} {
        redis_set $fd mykey foo
        redis_set $fd mykey2 bar
        redis_renamenx $fd mykey mykey2
    } {-ERR*}

    test {RENAMENX against already existing key (2)} {
        set res [redis_get $fd mykey]
        append res [redis_get $fd mykey2]
    } {foobar}

    test {RENAME against non existing source key} {
        redis_rename $fd nokey foobar
    } {-ERR*}

    test {RENAME where source and dest key is the same} {
        redis_rename $fd mykey mykey
    } {-ERR*}

    test {DEL all keys again (DB 0)} {
        foreach key [redis_keys $fd *] {
            redis_del $fd $key
        }
        redis_dbsize $fd
    } {0}

    test {DEL all keys again (DB 1)} {
        redis_select $fd 1
        foreach key [redis_keys $fd *] {
            redis_del $fd $key
        }
        set res [redis_dbsize $fd]
        redis_select $fd 0
        format $res
    } {0}


    test {MOVE basic usage} {
        redis_set $fd mykey foobar
        redis_move $fd mykey 1
        set res {}
        lappend res [redis_exists $fd mykey]
        lappend res [redis_dbsize $fd]
        redis_select $fd 1
        lappend res [redis_get $fd mykey]
        lappend res [redis_dbsize $fd]
        redis_select $fd 0
        format $res
    } [list 0 0 foobar 1]

    test {MOVE against key existing in the target DB} {
        redis_set $fd mykey hello
        redis_move $fd mykey 1
    } {-ERR*}

    test {SET/GET keys in different DBs} {
        redis_set $fd a hello
        redis_set $fd b world
        redis_select $fd 1
        redis_set $fd a foo
        redis_set $fd b bared
        redis_select $fd 0
        set res {}
        lappend res [redis_get $fd a]
        lappend res [redis_get $fd b]
        redis_select $fd 1
        lappend res [redis_get $fd a]
        lappend res [redis_get $fd b]
        redis_select $fd 0
        format $res
    } {hello world foo bared}

    test {Basic LPOP/RPOP} {
        redis_del $fd mylist
        redis_rpush $fd mylist 1
        redis_rpush $fd mylist 2
        redis_lpush $fd mylist 0
        list [redis_lpop $fd mylist] [redis_rpop $fd mylist] [redis_lpop $fd mylist] [redis_llen $fd mylist]
    } [list 0 2 1 0]

    test {LPOP/RPOP against empty list} {
        redis_lpop $fd mylist
    } {}

    test {LPOP against non list value} {
        redis_set $fd notalist foo
        redis_lpop $fd notalist
    } {*ERROR*POP against*}

    test {Mass LPUSH/LPOP} {
        set sum 0
        for {set i 0} {$i < 1000} {incr i} {
            redis_lpush $fd mylist $i
            incr sum $i
        }
        set sum2 0
        for {set i 0} {$i < 500} {incr i} {
            incr sum2 [redis_lpop $fd mylist]
            incr sum2 [redis_rpop $fd mylist]
        }
        expr $sum == $sum2
    } {1}

    test {LRANGE basics} {
        for {set i 0} {$i < 10} {incr i} {
            redis_rpush $fd mylist $i
        }
        list [redis_lrange $fd mylist 1 -2] \
                [redis_lrange $fd mylist -3 -1] \
                [redis_lrange $fd mylist 4 4]
    } {{1 2 3 4 5 6 7 8} {7 8 9} 4}

    test {LRANGE inverted indexes} {
        redis_lrange $fd mylist 6 2
    } {}

    test {LRANGE out of range indexes including the full list} {
        redis_lrange $fd mylist -1000 1000
    } {0 1 2 3 4 5 6 7 8 9}

    test {LRANGE against non existing key} {
        redis_lrange $fd nosuchkey 0 1
    } {}

    test {LTRIM basics} {
        redis_del $fd mylist
        for {set i 0} {$i < 100} {incr i} {
            redis_lpush $fd mylist $i
            redis_ltrim $fd mylist 0 4
        }
        redis_lrange $fd mylist 0 -1
    } {99 98 97 96 95}

    # Leave the user with a clean DB before to exit
    test {DEL all keys again (DB 0)} {
        foreach key [redis_keys $fd *] {
            redis_del $fd $key
        }
        redis_dbsize $fd
    } {0}

    test {DEL all keys again (DB 1)} {
        redis_select $fd 1
        foreach key [redis_keys $fd *] {
            redis_del $fd $key
        }
        set res [redis_dbsize $fd]
        redis_select $fd 0
        format $res
    } {0}


    puts "\n[expr $::passed+$::failed] tests, $::passed passed, $::failed failed"
    if {$::failed > 0} {
        puts "\n*** WARNING!!! $::failed FAILED TESTS ***\n"
    }
    close $fd
}

proc redis_connect {server port} {
    set fd [socket $server $port]
    fconfigure $fd -translation binary
    return $fd
}

proc redis_write {fd buf} {
    puts -nonewline $fd $buf
}

proc redis_writenl {fd buf} {
    # puts "C: $buf"
    redis_write $fd $buf
    redis_write $fd "\r\n"
    flush $fd
}

proc redis_readnl {fd len} {
    set buf [read $fd $len]
    read $fd 2 ; # discard CR LF
    return $buf
}

proc redis_bulk_read fd {
    set count [redis_read_integer $fd]
    if {$count eq {nil}} return {}
    set len [expr {abs($count)}]
    set buf [redis_readnl $fd $len]
    if {$count < 0} {return "***ERROR*** $buf"}
    return $buf
}

proc redis_multi_bulk_read fd {
    set count [redis_read_integer $fd]
    if {$count eq {nil}} return {}
    if {$count < 0} {
        set len [expr {abs($count)}]
        set buf [redis_readnl $fd $len]
        return "***ERROR*** $buf"
    }
    set l {}
    for {set i 0} {$i < $count} {incr i} {
        lappend l [redis_bulk_read $fd]
    }
    return $l
}

proc redis_read_retcode fd {
    set retcode [string trim [gets $fd]]
    # puts "S: $retcode"
    return $retcode
}

proc redis_read_integer fd {
    string trim [gets $fd]
}

### Actual API ###

proc redis_set {fd key val} {
    redis_writenl $fd "set $key [string length $val]\r\n$val"
    redis_read_retcode $fd
}

proc redis_setnx {fd key val} {
    redis_writenl $fd "setnx $key [string length $val]\r\n$val"
    redis_read_retcode $fd
}

proc redis_get {fd key} {
    redis_writenl $fd "get $key"
    redis_bulk_read $fd
}

proc redis_select {fd id} {
    redis_writenl $fd "select $id"
    redis_read_retcode $fd
}

proc redis_move {fd key id} {
    redis_writenl $fd "move $key $id"
    redis_read_retcode $fd
}

proc redis_del {fd key} {
    redis_writenl $fd "del $key"
    redis_read_retcode $fd
}

proc redis_keys {fd pattern} {
    redis_writenl $fd "keys $pattern"
    split [redis_bulk_read $fd]
}

proc redis_dbsize {fd} {
    redis_writenl $fd "dbsize"
    redis_read_integer $fd
}

proc redis_incr {fd key} {
    redis_writenl $fd "incr $key"
    redis_read_integer $fd
}

proc redis_decr {fd key} {
    redis_writenl $fd "decr $key"
    redis_read_integer $fd
}

proc redis_exists {fd key} {
    redis_writenl $fd "exists $key"
    redis_read_integer $fd
}

proc redis_lpush {fd key val} {
    redis_writenl $fd "lpush $key [string length $val]\r\n$val"
    redis_read_retcode $fd
}

proc redis_rpush {fd key val} {
    redis_writenl $fd "rpush $key [string length $val]\r\n$val"
    redis_read_retcode $fd
}

proc redis_llen {fd key} {
    redis_writenl $fd "llen $key"
    redis_read_integer $fd
}

proc redis_lindex {fd key index} {
    redis_writenl $fd "lindex $key $index"
    redis_bulk_read $fd
}

proc redis_lrange {fd key first last} {
    redis_writenl $fd "lrange $key $first $last"
    redis_multi_bulk_read $fd
}

proc redis_ltrim {fd key first last} {
    redis_writenl $fd "ltrim $key $first $last"
    redis_read_retcode $fd
}

proc redis_rename {fd key1 key2} {
    redis_writenl $fd "rename $key1 $key2"
    redis_read_retcode $fd
}

proc redis_renamenx {fd key1 key2} {
    redis_writenl $fd "renamenx $key1 $key2"
    redis_read_retcode $fd
}

proc redis_lpop {fd key} {
    redis_writenl $fd "lpop $key"
    redis_bulk_read $fd
}

proc redis_rpop {fd key} {
    redis_writenl $fd "rpop $key"
    redis_bulk_read $fd
}

if {[llength $argv] == 0} {
    main 127.0.0.1 6379
} else {
    main [lindex $argv 0] [lindex $argv 1]
}
