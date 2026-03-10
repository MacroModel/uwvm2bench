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
    (local $dp i32)
    (local $rep i32)
    (local $item i32)
    (local $cap i32)
    (local $weight i32)
    (local $value i32)
    (local $cand i32)
    (local $cur i32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $dp (i32.const 1024))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $rep (i32.const 0))
    (block $rep_done
      (loop $rep_loop
        (br_if $rep_done (i32.ge_u (local.get $rep) (i32.const 64)))

        (local.set $cap (i32.const 0))
        (block $zero_done
          (loop $zero_loop
            (br_if $zero_done (i32.gt_u (local.get $cap) (i32.const 2048)))
            (i32.store (i32.add (local.get $dp) (i32.shl (local.get $cap) (i32.const 2))) (i32.const 0))
            (local.set $cap (i32.add (local.get $cap) (i32.const 1)))
            (br $zero_loop)))

        (local.set $item (i32.const 0))
        (block $item_done
          (loop $item_loop
            (br_if $item_done (i32.ge_u (local.get $item) (i32.const 160)))
            (local.set $weight
              (i32.add
                (i32.const 1)
                (i32.and
                  (i32.add
                    (i32.mul (local.get $item) (i32.const 7))
                    (i32.mul (local.get $rep) (i32.const 3)))
                  (i32.const 63))))
            (local.set $value
              (i32.add
                (i32.const 3)
                (i32.and
                  (i32.add
                    (i32.mul (local.get $item) (i32.const 13))
                    (local.get $rep))
                  (i32.const 127))))
            (local.set $cap (i32.const 2048))
            (block $cap_done
              (loop $cap_loop
                (br_if $cap_done (i32.lt_u (local.get $cap) (local.get $weight)))
                (local.set $cur
                  (i32.load (i32.add (local.get $dp) (i32.shl (local.get $cap) (i32.const 2)))))
                (local.set $cand
                  (i32.add
                    (i32.load
                      (i32.add
                        (local.get $dp)
                        (i32.shl (i32.sub (local.get $cap) (local.get $weight)) (i32.const 2))))
                    (local.get $value)))
                (if (i32.gt_s (local.get $cand) (local.get $cur))
                  (then
                    (i32.store
                      (i32.add (local.get $dp) (i32.shl (local.get $cap) (i32.const 2)))
                      (local.get $cand))))
                (local.set $cap (i32.sub (local.get $cap) (i32.const 1)))
                (br $cap_loop)))
            (local.set $item (i32.add (local.get $item) (i32.const 1)))
            (br $item_loop)))

        (local.set $rep (i32.add (local.get $rep) (i32.const 1)))
        (br $rep_loop)))

    (i32.store (i32.const 64) (i32.load (i32.add (local.get $dp) (i32.const 8192))))

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
