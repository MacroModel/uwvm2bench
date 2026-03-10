(module
  (import "wasi_snapshot_preview1" "fd_write"
    (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "proc_exit"
    (func $proc_exit (param i32)))

  (memory (export "memory") 8)

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
    (local $abase i32)
    (local $bbase i32)
    (local $cbase i32)
    (local $rbase i32)
    (local $wb i32)
    (local $wd i32)
    (local $xbase i32)
    (local $sys i32)
    (local $i i32)
    (local $rep i32)
    (local $off i32)
    (local $m f32)
    (local $sum f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $abase (i32.const 1024))
    (local.set $bbase (i32.const 66560))
    (local.set $cbase (i32.const 132096))
    (local.set $rbase (i32.const 197632))
    (local.set $wb (i32.const 263168))
    (local.set $wd (i32.const 328704))
    (local.set $xbase (i32.const 394240))

    (local.set $sys (i32.const 0))
    (block $init_sys_done
      (loop $init_sys
        (br_if $init_sys_done (i32.ge_u (local.get $sys) (i32.const 64)))
        (local.set $i (i32.const 0))
        (block $init_i_done
          (loop $init_i
            (br_if $init_i_done (i32.ge_u (local.get $i) (i32.const 256)))
            (local.set $off
              (i32.shl
                (i32.add
                  (i32.mul (local.get $sys) (i32.const 256))
                  (local.get $i))
                (i32.const 2)))
            (f32.store (i32.add (local.get $abase) (local.get $off)) (f32.const -0.14))
            (f32.store
              (i32.add (local.get $bbase) (local.get $off))
              (f32.add
                (f32.const 2.4)
                (f32.mul
                  (f32.convert_i32_u (i32.and (i32.add (local.get $sys) (local.get $i)) (i32.const 7)))
                  (f32.const 0.03))))
            (f32.store (i32.add (local.get $cbase) (local.get $off)) (f32.const -0.12))
            (f32.store
              (i32.add (local.get $rbase) (local.get $off))
              (f32.add
                (f32.mul
                  (f32.convert_i32_u (i32.and (i32.mul (local.get $i) (i32.const 3)) (i32.const 31)))
                  (f32.const 0.02))
                (f32.mul
                  (f32.convert_i32_u (i32.and (i32.add (local.get $sys) (i32.const 5)) (i32.const 15)))
                  (f32.const 0.01))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $init_i)))
        (local.set $sys (i32.add (local.get $sys) (i32.const 1)))
        (br $init_sys)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $sum (f32.const 0.0))
    (local.set $rep (i32.const 0))
    (block $rep_done
      (loop $rep_loop
        (br_if $rep_done (i32.ge_u (local.get $rep) (i32.const 32)))
        (local.set $sys (i32.const 0))
        (block $sys_done
          (loop $sys_loop
            (br_if $sys_done (i32.ge_u (local.get $sys) (i32.const 64)))
            (local.set $i (i32.const 0))
            (block $copy_done
              (loop $copy_loop
                (br_if $copy_done (i32.ge_u (local.get $i) (i32.const 256)))
                (local.set $off
                  (i32.shl
                    (i32.add
                      (i32.mul (local.get $sys) (i32.const 256))
                      (local.get $i))
                    (i32.const 2)))
                (f32.store
                  (i32.add (local.get $wb) (local.get $off))
                  (f32.load (i32.add (local.get $bbase) (local.get $off))))
                (f32.store
                  (i32.add (local.get $wd) (local.get $off))
                  (f32.add
                    (f32.load (i32.add (local.get $rbase) (local.get $off)))
                    (f32.mul (f32.const 0.0008) (f32.convert_i32_u (local.get $rep)))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $copy_loop)))

            (local.set $i (i32.const 1))
            (block $fwd_done
              (loop $fwd_loop
                (br_if $fwd_done (i32.ge_u (local.get $i) (i32.const 256)))
                (local.set $off
                  (i32.shl
                    (i32.add
                      (i32.mul (local.get $sys) (i32.const 256))
                      (local.get $i))
                    (i32.const 2)))
                (local.set $m
                  (f32.div
                    (f32.load (i32.add (local.get $abase) (local.get $off)))
                    (f32.load (i32.sub (i32.add (local.get $wb) (local.get $off)) (i32.const 4)))))
                (f32.store
                  (i32.add (local.get $wb) (local.get $off))
                  (f32.sub
                    (f32.load (i32.add (local.get $wb) (local.get $off)))
                    (f32.mul
                      (local.get $m)
                      (f32.load (i32.sub (i32.add (local.get $cbase) (local.get $off)) (i32.const 4))))))
                (f32.store
                  (i32.add (local.get $wd) (local.get $off))
                  (f32.sub
                    (f32.load (i32.add (local.get $wd) (local.get $off)))
                    (f32.mul
                      (local.get $m)
                      (f32.load (i32.sub (i32.add (local.get $wd) (local.get $off)) (i32.const 4))))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $fwd_loop)))

            (local.set $off
              (i32.shl
                (i32.add
                  (i32.mul (local.get $sys) (i32.const 256))
                  (i32.const 255))
                (i32.const 2)))
            (f32.store
              (i32.add (local.get $xbase) (local.get $off))
              (f32.div
                (f32.load (i32.add (local.get $wd) (local.get $off)))
                (f32.load (i32.add (local.get $wb) (local.get $off)))))

            (local.set $i (i32.const 255))
            (block $back_done
              (loop $back_loop
                (br_if $back_done (i32.le_u (local.get $i) (i32.const 0)))
                (local.set $i (i32.sub (local.get $i) (i32.const 1)))
                (local.set $off
                  (i32.shl
                    (i32.add
                      (i32.mul (local.get $sys) (i32.const 256))
                      (local.get $i))
                    (i32.const 2)))
                (f32.store
                  (i32.add (local.get $xbase) (local.get $off))
                  (f32.div
                    (f32.sub
                      (f32.load (i32.add (local.get $wd) (local.get $off)))
                      (f32.mul
                        (f32.load (i32.add (local.get $cbase) (local.get $off)))
                        (f32.load (i32.add (i32.add (local.get $xbase) (local.get $off)) (i32.const 4)))))
                    (f32.load (i32.add (local.get $wb) (local.get $off)))))
                (br $back_loop)))

            (local.set $i (i32.const 0))
            (block $acc_done
              (loop $acc_loop
                (br_if $acc_done (i32.ge_u (local.get $i) (i32.const 256)))
                (local.set $off
                  (i32.shl
                    (i32.add
                      (i32.mul (local.get $sys) (i32.const 256))
                      (local.get $i))
                    (i32.const 2)))
                (local.set $sum
                  (f32.add
                    (local.get $sum)
                    (f32.load (i32.add (local.get $xbase) (local.get $off)))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $acc_loop)))
            (local.set $sys (i32.add (local.get $sys) (i32.const 1)))
            (br $sys_loop)))
        (local.set $rep (i32.add (local.get $rep) (i32.const 1)))
        (br $rep_loop)))
    (f32.store (i32.const 64) (local.get $sum))

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
