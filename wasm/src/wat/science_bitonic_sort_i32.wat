(module
  (import "wasi_snapshot_preview1" "fd_write"
    (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "proc_exit"
    (func $proc_exit (param i32)))

  (memory (export "memory") 1)

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

  (func (export "_start")
    (local $base i32)
    (local $work i32)
    (local $rep i32)
    (local $i i32)
    (local $k i32)
    (local $j i32)
    (local $ixj i32)
    (local $a i32)
    (local $b i32)
    (local $sum i64)
    (local $off i32)
    (local $dir i32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $base (i32.const 1024))
    (local.set $work (i32.const 12288))

    (local.set $i (i32.const 0))
    (block $init_done
      (loop $init
        (br_if $init_done (i32.ge_u (local.get $i) (i32.const 2048)))
        (i32.store
          (i32.add (local.get $base) (i32.shl (local.get $i) (i32.const 2)))
          (i32.sub
            (i32.and
              (i32.add
                (i32.mul (local.get $i) (i32.const 1103515245))
                (i32.const 12345))
              (i32.const 65535))
            (i32.const 32768)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $init)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $sum (i64.const 0))
    (local.set $rep (i32.const 0))
    (block $rep_done
      (loop $rep_loop
        (br_if $rep_done (i32.ge_u (local.get $rep) (i32.const 40)))
        (local.set $i (i32.const 0))
        (block $copy_done
          (loop $copy_loop
            (br_if $copy_done (i32.ge_u (local.get $i) (i32.const 2048)))
            (local.set $off (i32.shl (local.get $i) (i32.const 2)))
            (i32.store
              (i32.add (local.get $work) (local.get $off))
              (i32.add
                (i32.load (i32.add (local.get $base) (local.get $off)))
                (i32.mul (local.get $rep) (i32.const 3))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $copy_loop)))

        (local.set $k (i32.const 2))
        (block $k_done
          (loop $k_loop
            (br_if $k_done (i32.gt_u (local.get $k) (i32.const 2048)))
            (local.set $j (i32.shr_u (local.get $k) (i32.const 1)))
            (block $j_done
              (loop $j_loop
                (br_if $j_done (i32.eqz (local.get $j)))
                (local.set $i (i32.const 0))
                (block $i_done
                  (loop $i_loop
                    (br_if $i_done (i32.ge_u (local.get $i) (i32.const 2048)))
                    (local.set $ixj (i32.xor (local.get $i) (local.get $j)))
                    (if (i32.gt_u (local.get $ixj) (local.get $i))
                      (then
                        (local.set $a
                          (i32.load (i32.add (local.get $work) (i32.shl (local.get $i) (i32.const 2)))))
                        (local.set $b
                          (i32.load (i32.add (local.get $work) (i32.shl (local.get $ixj) (i32.const 2)))))
                        (local.set $dir
                          (if (result i32)
                            (i32.eqz (i32.and (local.get $i) (local.get $k)))
                            (then (i32.const 1))
                            (else (i32.const 0))))
                        (if
                          (i32.or
                            (i32.and (local.get $dir) (i32.gt_s (local.get $a) (local.get $b)))
                            (i32.and (i32.eqz (local.get $dir)) (i32.lt_s (local.get $a) (local.get $b))))
                          (then
                            (i32.store (i32.add (local.get $work) (i32.shl (local.get $i) (i32.const 2))) (local.get $b))
                            (i32.store (i32.add (local.get $work) (i32.shl (local.get $ixj) (i32.const 2))) (local.get $a))))))
                    (local.set $i (i32.add (local.get $i) (i32.const 1)))
                    (br $i_loop)))
                (local.set $j (i32.shr_u (local.get $j) (i32.const 1)))
                (br $j_loop)))
            (local.set $k (i32.shl (local.get $k) (i32.const 1)))
            (br $k_loop)))

        (local.set $i (i32.const 0))
        (block $sum_done
          (loop $sum_loop
            (br_if $sum_done (i32.ge_u (local.get $i) (i32.const 2048)))
            (if
              (i32.eqz (i32.and (local.get $i) (i32.const 63)))
              (then
                (local.set $sum
                  (i64.add
                    (local.get $sum)
                    (i64.extend_i32_s
                      (i32.load (i32.add (local.get $work) (i32.shl (local.get $i) (i32.const 2)))))))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $sum_loop)))
        (local.set $rep (i32.add (local.get $rep) (i32.const 1)))
        (br $rep_loop)))
    (i64.store (i32.const 64) (local.get $sum))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 256))
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
