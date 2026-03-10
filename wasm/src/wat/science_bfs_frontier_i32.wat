(module
  (import "wasi_snapshot_preview1" "fd_write"
    (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "proc_exit"
    (func $proc_exit (param i32)))

  (memory (export "memory") 2)

  (func $write (param $ptr i32) (param $len i32)
    (i32.store (i32.const 0) (local.get $ptr))
    (i32.store (i32.const 4) (local.get $len))
    (call $fd_write (i32.const 1) (i32.const 0) (i32.const 1) (i32.const 8))
    drop)

  (func $write_u64_dec (param $n i64) (param $out i32) (result i32)
    (local $scratch i32)
    (local $p i32)
    (local $len i32)
    (local $i i32)
    (if (i64.eq (local.get $n) (i64.const 0))
      (then
        (i32.store8 (local.get $out) (i32.const 48))
        (return (i32.const 1))))
    (local.set $scratch (i32.add (local.get $out) (i32.const 32)))
    (local.set $p (local.get $scratch))
    (block $done
      (loop $loop
        (br_if $done (i64.eq (local.get $n) (i64.const 0)))
        (local.set $p (i32.sub (local.get $p) (i32.const 1)))
        (i32.store8
          (local.get $p)
          (i32.add (i32.wrap_i64 (i64.rem_u (local.get $n) (i64.const 10))) (i32.const 48)))
        (local.set $n (i64.div_u (local.get $n) (i64.const 10)))
        (br $loop)))
    (local.set $len (i32.sub (local.get $scratch) (local.get $p)))
    (local.set $i (i32.const 0))
    (block $copy_done
      (loop $copy
        (br_if $copy_done (i32.ge_u (local.get $i) (local.get $len)))
        (i32.store8
          (i32.add (local.get $out) (local.get $i))
          (i32.load8_u (i32.add (local.get $p) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $copy)))
    (local.get $len))

  (func $write_u32_pad3 (param $v i32) (param $out i32)
    (i32.store8 (local.get $out) (i32.add (i32.div_u (local.get $v) (i32.const 100)) (i32.const 48)))
    (i32.store8
      (i32.add (local.get $out) (i32.const 1))
      (i32.add (i32.rem_u (i32.div_u (local.get $v) (i32.const 10)) (i32.const 10)) (i32.const 48)))
    (i32.store8 (i32.add (local.get $out) (i32.const 2)) (i32.add (i32.rem_u (local.get $v) (i32.const 10)) (i32.const 48))))

  (func $visit (param $dist_base i32) (param $next_base i32) (param $node i32) (param $depth i32) (result i32)
    (local $addr i32)
    (local.set $addr (i32.add (local.get $dist_base) (i32.shl (local.get $node) (i32.const 2))))
    (if (result i32)
      (i32.eq (i32.load (local.get $addr)) (i32.const -1))
      (then
        (i32.store (local.get $addr) (i32.add (local.get $depth) (i32.const 1)))
        (i32.store (i32.add (local.get $next_base) (i32.shl (local.get $node) (i32.const 2))) (i32.const 1))
        (i32.const 1))
      (else (i32.const 0))))

  (func (export "_start")
    (local $distbase i32)
    (local $frontbase i32)
    (local $nextbase i32)
    (local $run i32)
    (local $depth i32)
    (local $i i32)
    (local $root i32)
    (local $frontier_count i32)
    (local $node i32)
    (local $total i64)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $distbase (i32.const 1024))
    (local.set $frontbase (i32.const 9216))
    (local.set $nextbase (i32.const 17408))
    (local.set $total (i64.const 0))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $run (i32.const 0))
    (block $run_done
      (loop $run_loop
        (br_if $run_done (i32.ge_u (local.get $run) (i32.const 40)))
        (local.set $i (i32.const 0))
        (block $init_done
          (loop $init
            (br_if $init_done (i32.ge_u (local.get $i) (i32.const 2048)))
            (i32.store (i32.add (local.get $distbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const -1))
            (i32.store (i32.add (local.get $frontbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
            (i32.store (i32.add (local.get $nextbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $init)))
        (local.set $root
          (i32.and
            (i32.add (i32.mul (local.get $run) (i32.const 37)) (i32.const 5))
            (i32.const 2047)))
        (i32.store (i32.add (local.get $distbase) (i32.shl (local.get $root) (i32.const 2))) (i32.const 0))
        (i32.store (i32.add (local.get $frontbase) (i32.shl (local.get $root) (i32.const 2))) (i32.const 1))
        (local.set $depth (i32.const 0))
        (block $depth_done
          (loop $depth_loop
            (br_if $depth_done (i32.ge_u (local.get $depth) (i32.const 24)))
            (local.set $frontier_count (i32.const 0))
            (local.set $i (i32.const 0))
            (block $scan_done
              (loop $scan
                (br_if $scan_done (i32.ge_u (local.get $i) (i32.const 2048)))
                (if
                  (i32.ne
                    (i32.load (i32.add (local.get $frontbase) (i32.shl (local.get $i) (i32.const 2))))
                    (i32.const 0))
                  (then
                    (i32.store (i32.add (local.get $frontbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
                    (local.set $node (i32.and (i32.add (local.get $i) (i32.const 1)) (i32.const 2047)))
                    (local.set $frontier_count
                      (i32.add
                        (local.get $frontier_count)
                        (call $visit (local.get $distbase) (local.get $nextbase) (local.get $node) (local.get $depth))))
                    (local.set $node (i32.and (i32.add (local.get $i) (i32.const 63)) (i32.const 2047)))
                    (local.set $frontier_count
                      (i32.add
                        (local.get $frontier_count)
                        (call $visit (local.get $distbase) (local.get $nextbase) (local.get $node) (local.get $depth))))
                    (local.set $node
                      (i32.and
                        (i32.add
                          (i32.add (i32.mul (local.get $i) (i32.const 17)) (i32.mul (local.get $run) (i32.const 9)))
                          (i32.const 3))
                        (i32.const 2047)))
                    (local.set $frontier_count
                      (i32.add
                        (local.get $frontier_count)
                        (call $visit (local.get $distbase) (local.get $nextbase) (local.get $node) (local.get $depth))))
                    (local.set $node
                      (i32.and
                        (i32.add
                          (i32.add (i32.mul (local.get $i) (i32.const 29)) (i32.mul (local.get $depth) (i32.const 11)))
                          (i32.const 7))
                        (i32.const 2047)))
                    (local.set $frontier_count
                      (i32.add
                        (local.get $frontier_count)
                        (call $visit (local.get $distbase) (local.get $nextbase) (local.get $node) (local.get $depth))))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $scan)))
            (br_if $depth_done (i32.eqz (local.get $frontier_count)))
            (local.set $i (i32.const 0))
            (block $swap_done
              (loop $swap
                (br_if $swap_done (i32.ge_u (local.get $i) (i32.const 2048)))
                (i32.store
                  (i32.add (local.get $frontbase) (i32.shl (local.get $i) (i32.const 2)))
                  (i32.load (i32.add (local.get $nextbase) (i32.shl (local.get $i) (i32.const 2)))))
                (i32.store (i32.add (local.get $nextbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $swap)))
            (local.set $depth (i32.add (local.get $depth) (i32.const 1)))
            (br $depth_loop)))
        (local.set $i (i32.const 0))
        (block $sum_done
          (loop $sum
            (br_if $sum_done (i32.ge_u (local.get $i) (i32.const 2048)))
            (if
              (i32.ge_s
                (i32.load (i32.add (local.get $distbase) (i32.shl (local.get $i) (i32.const 2))))
                (i32.const 0))
              (then
                (local.set $total
                  (i64.add
                    (local.get $total)
                    (i64.extend_i32_s
                      (i32.load (i32.add (local.get $distbase) (i32.shl (local.get $i) (i32.const 2)))))))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $sum)))
        (local.set $run (i32.add (local.get $run) (i32.const 1)))
        (br $run_loop)))

    (i64.store (i32.const 64) (local.get $total))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 25600))
    (i32.store8 (i32.add (local.get $p) (i32.const 0)) (i32.const 84))
    (i32.store8 (i32.add (local.get $p) (i32.const 1)) (i32.const 105))
    (i32.store8 (i32.add (local.get $p) (i32.const 2)) (i32.const 109))
    (i32.store8 (i32.add (local.get $p) (i32.const 3)) (i32.const 101))
    (i32.store8 (i32.add (local.get $p) (i32.const 4)) (i32.const 58))
    (i32.store8 (i32.add (local.get $p) (i32.const 5)) (i32.const 32))
    (local.set $nlen (call $write_u64_dec (local.get $ms_int) (i32.add (local.get $p) (i32.const 6))))
    (i32.store8 (i32.add (local.get $p) (i32.add (i32.const 6) (local.get $nlen))) (i32.const 46))
    (call $write_u32_pad3 (local.get $ms_frac) (i32.add (local.get $p) (i32.add (i32.const 7) (local.get $nlen))))
    (i32.store8 (i32.add (local.get $p) (i32.add (i32.const 10) (local.get $nlen))) (i32.const 32))
    (i32.store8 (i32.add (local.get $p) (i32.add (i32.const 11) (local.get $nlen))) (i32.const 109))
    (i32.store8 (i32.add (local.get $p) (i32.add (i32.const 12) (local.get $nlen))) (i32.const 115))
    (i32.store8 (i32.add (local.get $p) (i32.add (i32.const 13) (local.get $nlen))) (i32.const 10))
    (call $write (local.get $p) (i32.add (i32.const 14) (local.get $nlen)))
    (call $proc_exit (i32.const 0)))
)
