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
    (local $fbase i32)
    (local $xbase i32)
    (local $pxbase i32)
    (local $pybase i32)
    (local $iter i32)
    (local $y i32)
    (local $x i32)
    (local $idx i32)
    (local $off i32)
    (local $cur f32)
    (local $right f32)
    (local $down f32)
    (local $gx f32)
    (local $gy f32)
    (local $px f32)
    (local $py f32)
    (local $den f32)
    (local $div f32)
    (local $fval f32)
    (local $xn f32)
    (local $sum f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $fbase (i32.const 1024))
    (local.set $xbase (i32.const 37888))
    (local.set $pxbase (i32.const 74752))
    (local.set $pybase (i32.const 111616))

    (local.set $y (i32.const 0))
    (block $init_y_done
      (loop $init_y
        (br_if $init_y_done (i32.ge_u (local.get $y) (i32.const 96)))
        (local.set $x (i32.const 0))
        (block $init_x_done
          (loop $init_x
            (br_if $init_x_done (i32.ge_u (local.get $x) (i32.const 96)))
            (local.set $idx (i32.add (i32.mul (local.get $y) (i32.const 96)) (local.get $x)))
            (local.set $off (i32.shl (local.get $idx) (i32.const 2)))
            (local.set $fval
              (f32.add
                (f32.const 0.55)
                (f32.add
                  (f32.mul (f32.convert_i32_u (i32.and (local.get $x) (i32.const 15))) (f32.const 0.012))
                  (f32.mul (f32.convert_i32_u (i32.and (local.get $y) (i32.const 15))) (f32.const -0.008)))))
            (f32.store (i32.add (local.get $fbase) (local.get $off)) (local.get $fval))
            (f32.store (i32.add (local.get $xbase) (local.get $off)) (local.get $fval))
            (f32.store (i32.add (local.get $pxbase) (local.get $off)) (f32.const 0.0))
            (f32.store (i32.add (local.get $pybase) (local.get $off)) (f32.const 0.0))
            (local.set $x (i32.add (local.get $x) (i32.const 1)))
            (br $init_x)))
        (local.set $y (i32.add (local.get $y) (i32.const 1)))
        (br $init_y)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $iter (i32.const 0))
    (block $iter_done
      (loop $iter_loop
        (br_if $iter_done (i32.ge_u (local.get $iter) (i32.const 72)))
        (local.set $y (i32.const 0))
        (block $dual_y_done
          (loop $dual_y
            (br_if $dual_y_done (i32.ge_u (local.get $y) (i32.const 96)))
            (local.set $x (i32.const 0))
            (block $dual_x_done
              (loop $dual_x
                (br_if $dual_x_done (i32.ge_u (local.get $x) (i32.const 96)))
                (local.set $idx (i32.add (i32.mul (local.get $y) (i32.const 96)) (local.get $x)))
                (local.set $off (i32.shl (local.get $idx) (i32.const 2)))
                (local.set $cur (f32.load (i32.add (local.get $xbase) (local.get $off))))
                (local.set $right
                  (if (result f32) (i32.lt_u (local.get $x) (i32.const 95))
                    (then
                      (f32.load
                        (i32.add
                          (local.get $xbase)
                          (i32.shl (i32.add (local.get $idx) (i32.const 1)) (i32.const 2)))))
                    (else (local.get $cur))))
                (local.set $down
                  (if (result f32) (i32.lt_u (local.get $y) (i32.const 95))
                    (then
                      (f32.load
                        (i32.add
                          (local.get $xbase)
                          (i32.shl (i32.add (local.get $idx) (i32.const 96)) (i32.const 2)))))
                    (else (local.get $cur))))
                (local.set $gx (f32.sub (local.get $right) (local.get $cur)))
                (local.set $gy (f32.sub (local.get $down) (local.get $cur)))
                (local.set $px
                  (f32.add
                    (f32.load (i32.add (local.get $pxbase) (local.get $off)))
                    (f32.mul (f32.const 0.22) (local.get $gx))))
                (local.set $py
                  (f32.add
                    (f32.load (i32.add (local.get $pybase) (local.get $off)))
                    (f32.mul (f32.const 0.22) (local.get $gy))))
                (local.set $den
                  (f32.add
                    (f32.const 1.0)
                    (f32.add (f32.abs (local.get $px)) (f32.abs (local.get $py)))))
                (f32.store (i32.add (local.get $pxbase) (local.get $off)) (f32.div (local.get $px) (local.get $den)))
                (f32.store (i32.add (local.get $pybase) (local.get $off)) (f32.div (local.get $py) (local.get $den)))
                (local.set $x (i32.add (local.get $x) (i32.const 1)))
                (br $dual_x)))
            (local.set $y (i32.add (local.get $y) (i32.const 1)))
            (br $dual_y)))

        (local.set $y (i32.const 0))
        (block $primal_y_done
          (loop $primal_y
            (br_if $primal_y_done (i32.ge_u (local.get $y) (i32.const 96)))
            (local.set $x (i32.const 0))
            (block $primal_x_done
              (loop $primal_x
                (br_if $primal_x_done (i32.ge_u (local.get $x) (i32.const 96)))
                (local.set $idx (i32.add (i32.mul (local.get $y) (i32.const 96)) (local.get $x)))
                (local.set $off (i32.shl (local.get $idx) (i32.const 2)))
                (local.set $div (f32.load (i32.add (local.get $pxbase) (local.get $off))))
                (if (i32.gt_u (local.get $x) (i32.const 0))
                  (then
                    (local.set $div
                      (f32.sub
                        (local.get $div)
                        (f32.load
                          (i32.add
                            (local.get $pxbase)
                            (i32.shl (i32.sub (local.get $idx) (i32.const 1)) (i32.const 2))))))))
                (local.set $div
                  (f32.add
                    (local.get $div)
                    (f32.load (i32.add (local.get $pybase) (local.get $off)))))
                (if (i32.gt_u (local.get $y) (i32.const 0))
                  (then
                    (local.set $div
                      (f32.sub
                        (local.get $div)
                        (f32.load
                          (i32.add
                            (local.get $pybase)
                            (i32.shl (i32.sub (local.get $idx) (i32.const 96)) (i32.const 2))))))))
                (local.set $cur (f32.load (i32.add (local.get $xbase) (local.get $off))))
                (local.set $fval (f32.load (i32.add (local.get $fbase) (local.get $off))))
                (local.set $xn
                  (f32.sub
                    (f32.add (local.get $cur) (f32.mul (f32.const 0.16) (local.get $div)))
                    (f32.mul (f32.const 0.08) (f32.sub (local.get $cur) (local.get $fval)))))
                (local.set $xn
                  (if (result f32) (f32.lt (local.get $xn) (f32.const 0.0))
                    (then (f32.const 0.0))
                    (else
                      (if (result f32) (f32.gt (local.get $xn) (f32.const 1.4))
                        (then (f32.const 1.4))
                        (else (local.get $xn))))))
                (f32.store
                  (i32.add (local.get $xbase) (local.get $off))
                  (f32.add
                    (f32.mul (f32.const 0.95) (local.get $cur))
                    (f32.mul (f32.const 0.05) (local.get $xn))))
                (local.set $x (i32.add (local.get $x) (i32.const 1)))
                (br $primal_x)))
            (local.set $y (i32.add (local.get $y) (i32.const 1)))
            (br $primal_y)))
        (local.set $iter (i32.add (local.get $iter) (i32.const 1)))
        (br $iter_loop)))

    (local.set $sum (f32.const 0.0))
    (local.set $idx (i32.const 0))
    (block $sum_done
      (loop $sum_loop
        (br_if $sum_done (i32.ge_u (local.get $idx) (i32.const 9216)))
        (local.set $sum
          (f32.add
            (local.get $sum)
            (f32.load (i32.add (local.get $xbase) (i32.shl (local.get $idx) (i32.const 2))))))
        (local.set $idx (i32.add (local.get $idx) (i32.const 1)))
        (br $sum_loop)))
    (f32.store (i32.const 64) (local.get $sum))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 148480))
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
