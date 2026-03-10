(module
  (import "wasi_snapshot_preview1" "fd_write"
    (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "proc_exit"
    (func $proc_exit (param i32)))

  (memory (export "memory") 4)

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
    (local $coeff i32)
    (local $input i32)
    (local $n i32)
    (local $k i32)
    (local $rep i32)
    (local $sum f32)
    (local $acc f32)
    (local $ptr i32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $coeff (i32.const 1024))
    (local.set $input (i32.const 2048))

    (local.set $k (i32.const 0))
    (block $init_coeff_done
      (loop $init_coeff
        (br_if $init_coeff_done (i32.ge_u (local.get $k) (i32.const 32)))
        (f32.store
          (i32.add (local.get $coeff) (i32.shl (local.get $k) (i32.const 2)))
          (f32.div
            (f32.convert_i32_u
              (i32.sub (i32.const 32) (local.get $k)))
            (f32.const 528.0)))
        (local.set $k (i32.add (local.get $k) (i32.const 1)))
        (br $init_coeff)))

    (local.set $n (i32.const 0))
    (block $init_input_done
      (loop $init_input
        (br_if $init_input_done (i32.ge_u (local.get $n) (i32.const 32768)))
        (f32.store
          (i32.add (local.get $input) (i32.shl (local.get $n) (i32.const 2)))
          (f32.add
            (f32.mul (f32.convert_i32_u (i32.and (local.get $n) (i32.const 63))) (f32.const 0.0078125))
            (f32.mul (f32.convert_i32_u (i32.and (i32.mul (local.get $n) (i32.const 5)) (i32.const 31))) (f32.const 0.00390625))))
        (local.set $n (i32.add (local.get $n) (i32.const 1)))
        (br $init_input)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $acc (f32.const 0.0))
    (local.set $rep (i32.const 0))
    (block $rep_done
      (loop $rep_loop
        (br_if $rep_done (i32.ge_u (local.get $rep) (i32.const 24)))
        (local.set $n (i32.const 31))
        (block $sample_done
          (loop $sample_loop
            (br_if $sample_done (i32.ge_u (local.get $n) (i32.const 32768)))
            (local.set $sum (f32.const 0.0))
            (local.set $k (i32.const 0))
            (block $tap_done
              (loop $tap_loop
                (br_if $tap_done (i32.ge_u (local.get $k) (i32.const 32)))
                (local.set $sum
                  (f32.add
                    (local.get $sum)
                    (f32.mul
                      (f32.load (i32.add (local.get $coeff) (i32.shl (local.get $k) (i32.const 2))))
                      (f32.load
                        (i32.add
                          (local.get $input)
                          (i32.shl (i32.sub (local.get $n) (local.get $k)) (i32.const 2)))))))
                (local.set $k (i32.add (local.get $k) (i32.const 1)))
                (br $tap_loop)))
            (local.set $acc (f32.add (local.get $acc) (local.get $sum)))
            (local.set $n (i32.add (local.get $n) (i32.const 1)))
            (br $sample_loop)))
        (local.set $rep (i32.add (local.get $rep) (i32.const 1)))
        (br $rep_loop)))
    (f32.store (i32.const 64) (local.get $acc))

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
