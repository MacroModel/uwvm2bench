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

  (func $soft_thresh (param $x f32) (param $lambda f32) (result f32)
    (if (result f32) (f32.gt (local.get $x) (local.get $lambda))
      (then (f32.sub (local.get $x) (local.get $lambda)))
      (else
        (if (result f32) (f32.lt (local.get $x) (f32.neg (local.get $lambda)))
          (then (f32.add (local.get $x) (local.get $lambda)))
          (else (f32.const 0.0))))))

  (func (export "_start")
    (local $xbase i32)
    (local $zbase i32)
    (local $ubase i32)
    (local $tbase i32)
    (local $iter i32)
    (local $i i32)
    (local $j i32)
    (local $off i32)
    (local $diag f32)
    (local $rhs f32)
    (local $coeff f32)
    (local $xv f32)
    (local $zv f32)
    (local $sum f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $xbase (i32.const 1024))
    (local.set $zbase (i32.const 1536))
    (local.set $ubase (i32.const 2048))
    (local.set $tbase (i32.const 2560))

    (local.set $i (i32.const 0))
    (block $init_done
      (loop $init
        (br_if $init_done (i32.ge_u (local.get $i) (i32.const 96)))
        (local.set $off (i32.shl (local.get $i) (i32.const 2)))
        (f32.store (i32.add (local.get $xbase) (local.get $off)) (f32.const 0.0))
        (f32.store (i32.add (local.get $zbase) (local.get $off)) (f32.const 0.0))
        (f32.store (i32.add (local.get $ubase) (local.get $off)) (f32.const 0.0))
        (f32.store
          (i32.add (local.get $tbase) (local.get $off))
          (f32.mul
            (f32.convert_i32_s
              (i32.sub
                (i32.and
                  (i32.add (i32.mul (local.get $i) (i32.const 7)) (i32.const 3))
                  (i32.const 15))
                (i32.const 8)))
            (f32.const 0.055)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $init)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $iter (i32.const 0))
    (block $iter_done
      (loop $iter_loop
        (br_if $iter_done (i32.ge_u (local.get $iter) (i32.const 180)))
        (local.set $i (i32.const 0))
        (block $x_done
          (loop $x_loop
            (br_if $x_done (i32.ge_u (local.get $i) (i32.const 96)))
            (local.set $off (i32.shl (local.get $i) (i32.const 2)))
            (local.set $diag
              (f32.add
                (f32.const 1.80)
                (f32.mul
                  (f32.convert_i32_u (i32.and (i32.add (local.get $i) (local.get $iter)) (i32.const 7)))
                  (f32.const 0.05))))
            (local.set $rhs
              (f32.add
                (f32.load (i32.add (local.get $tbase) (local.get $off)))
                (f32.mul
                  (f32.const 1.15)
                  (f32.sub
                    (f32.load (i32.add (local.get $zbase) (local.get $off)))
                    (f32.load (i32.add (local.get $ubase) (local.get $off)))))))
            (local.set $j (i32.const 0))
            (block $j_done
              (loop $j_loop
                (br_if $j_done (i32.ge_u (local.get $j) (i32.const 96)))
                (if (i32.ne (local.get $j) (local.get $i))
                  (then
                    (local.set $coeff
                      (f32.mul
                        (f32.convert_i32_s
                          (i32.sub
                            (i32.and
                              (i32.add
                                (i32.add
                                  (i32.mul (local.get $i) (i32.const 9))
                                  (i32.mul (local.get $j) (i32.const 5)))
                                (local.get $iter))
                              (i32.const 7))
                            (i32.const 3)))
                        (f32.const 0.015)))
                    (local.set $rhs
                      (f32.sub
                        (local.get $rhs)
                        (f32.mul
                          (local.get $coeff)
                          (f32.load (i32.add (local.get $zbase) (i32.shl (local.get $j) (i32.const 2)))))))))
                (local.set $j (i32.add (local.get $j) (i32.const 1)))
                (br $j_loop)))
            (f32.store
              (i32.add (local.get $xbase) (local.get $off))
              (f32.div (local.get $rhs) (local.get $diag)))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $x_loop)))
        (local.set $i (i32.const 0))
        (block $z_done
          (loop $z_loop
            (br_if $z_done (i32.ge_u (local.get $i) (i32.const 96)))
            (local.set $off (i32.shl (local.get $i) (i32.const 2)))
            (local.set $xv
              (f32.add
                (f32.load (i32.add (local.get $xbase) (local.get $off)))
                (f32.load (i32.add (local.get $ubase) (local.get $off)))))
            (local.set $zv
              (call $soft_thresh (local.get $xv) (f32.const 0.08)))
            (local.set $zv
              (f32.add
                (f32.mul (f32.const 0.93) (f32.load (i32.add (local.get $zbase) (local.get $off))))
                (f32.mul (f32.const 0.07) (local.get $zv))))
            (f32.store (i32.add (local.get $zbase) (local.get $off)) (local.get $zv))
            (f32.store
              (i32.add (local.get $ubase) (local.get $off))
              (f32.add
                (f32.load (i32.add (local.get $ubase) (local.get $off)))
                (f32.sub (f32.load (i32.add (local.get $xbase) (local.get $off))) (local.get $zv))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $z_loop)))
        (local.set $iter (i32.add (local.get $iter) (i32.const 1)))
        (br $iter_loop)))

    (local.set $sum (f32.const 0.0))
    (local.set $i (i32.const 0))
    (block $sum_done
      (loop $sum_loop
        (br_if $sum_done (i32.ge_u (local.get $i) (i32.const 96)))
        (local.set $sum
          (f32.add
            (local.get $sum)
            (f32.abs (f32.load (i32.add (local.get $zbase) (i32.shl (local.get $i) (i32.const 2)))))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $sum_loop)))
    (f32.store (i32.const 64) (local.get $sum))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 3072))
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
