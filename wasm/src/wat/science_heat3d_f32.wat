(module
  (import "wasi_snapshot_preview1" "fd_write"
    (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "proc_exit"
    (func $proc_exit (param i32)))

  (memory (export "memory") 5)

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
    (local $src i32)
    (local $dst i32)
    (local $tmp i32)
    (local $z i32)
    (local $y i32)
    (local $x i32)
    (local $idx i32)
    (local $step i32)
    (local $sum f32)
    (local $val f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $src (i32.const 1024))
    (local.set $dst (i32.const 140000))

    (local.set $z (i32.const 10))
    (block $init_z_done
      (loop $init_z
        (br_if $init_z_done (i32.ge_u (local.get $z) (i32.const 22)))
        (local.set $y (i32.const 10))
        (block $init_y_done
          (loop $init_y
            (br_if $init_y_done (i32.ge_u (local.get $y) (i32.const 22)))
            (local.set $x (i32.const 10))
            (block $init_x_done
              (loop $init_x
                (br_if $init_x_done (i32.ge_u (local.get $x) (i32.const 22)))
                (local.set $idx
                  (i32.add
                    (local.get $src)
                    (i32.shl
                      (i32.add
                        (i32.add
                          (i32.mul (local.get $z) (i32.const 1024))
                          (i32.mul (local.get $y) (i32.const 32)))
                        (local.get $x))
                      (i32.const 2))))
                (f32.store (local.get $idx) (f32.const 1.0))
                (local.set $x (i32.add (local.get $x) (i32.const 1)))
                (br $init_x)))
            (local.set $y (i32.add (local.get $y) (i32.const 1)))
            (br $init_y)))
        (local.set $z (i32.add (local.get $z) (i32.const 1)))
        (br $init_z)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $step (i32.const 0))
    (block $step_done
      (loop $step_loop
        (br_if $step_done (i32.ge_u (local.get $step) (i32.const 32)))
        (local.set $z (i32.const 1))
        (block $z_done
          (loop $z_loop
            (br_if $z_done (i32.ge_u (local.get $z) (i32.const 31)))
            (local.set $y (i32.const 1))
            (block $y_done
              (loop $y_loop
                (br_if $y_done (i32.ge_u (local.get $y) (i32.const 31)))
                (local.set $x (i32.const 1))
                (block $x_done
                  (loop $x_loop
                    (br_if $x_done (i32.ge_u (local.get $x) (i32.const 31)))
                    (local.set $idx
                      (i32.add
                        (local.get $src)
                        (i32.shl
                          (i32.add
                            (i32.add
                              (i32.mul (local.get $z) (i32.const 1024))
                            (i32.mul (local.get $y) (i32.const 32)))
                            (local.get $x))
                          (i32.const 2))))
                    (local.set $val
                      (f32.add
                        (f32.add
                          (f32.mul (f32.const 0.40) (f32.load (local.get $idx)))
                          (f32.mul
                            (f32.const 0.10)
                            (f32.add
                              (f32.add
                                (f32.load (i32.sub (local.get $idx) (i32.const 4)))
                                (f32.load (i32.add (local.get $idx) (i32.const 4))))
                              (f32.add
                                (f32.load (i32.sub (local.get $idx) (i32.const 128)))
                                (f32.load (i32.add (local.get $idx) (i32.const 128)))))))
                        (f32.mul
                          (f32.const 0.10)
                          (f32.add
                            (f32.load (i32.sub (local.get $idx) (i32.const 4096)))
                            (f32.load (i32.add (local.get $idx) (i32.const 4096)))))))
                    (f32.store
                      (i32.add
                        (local.get $dst)
                        (i32.shl
                          (i32.add
                            (i32.add
                              (i32.mul (local.get $z) (i32.const 1024))
                              (i32.mul (local.get $y) (i32.const 32)))
                            (local.get $x))
                          (i32.const 2)))
                      (local.get $val))
                    (local.set $x (i32.add (local.get $x) (i32.const 1)))
                    (br $x_loop)))
                (local.set $y (i32.add (local.get $y) (i32.const 1)))
                (br $y_loop)))
            (local.set $z (i32.add (local.get $z) (i32.const 1)))
            (br $z_loop)))
        (local.set $tmp (local.get $src))
        (local.set $src (local.get $dst))
        (local.set $dst (local.get $tmp))
        (local.set $step (i32.add (local.get $step) (i32.const 1)))
        (br $step_loop)))

    (local.set $sum (f32.const 0.0))
    (local.set $z (i32.const 1))
    (block $sum_z_done
      (loop $sum_z
        (br_if $sum_z_done (i32.ge_u (local.get $z) (i32.const 31)))
        (local.set $y (i32.const 1))
        (block $sum_y_done
          (loop $sum_y
            (br_if $sum_y_done (i32.ge_u (local.get $y) (i32.const 31)))
            (local.set $x (i32.const 1))
            (block $sum_x_done
              (loop $sum_x
                (br_if $sum_x_done (i32.ge_u (local.get $x) (i32.const 31)))
                (local.set $sum
                  (f32.add
                    (local.get $sum)
                    (f32.load
                      (i32.add
                        (local.get $src)
                        (i32.shl
                          (i32.add
                            (i32.add
                              (i32.mul (local.get $z) (i32.const 1024))
                              (i32.mul (local.get $y) (i32.const 32)))
                            (local.get $x))
                          (i32.const 2))))))
                (local.set $x (i32.add (local.get $x) (i32.const 1)))
                (br $sum_x)))
            (local.set $y (i32.add (local.get $y) (i32.const 1)))
            (br $sum_y)))
        (local.set $z (i32.add (local.get $z) (i32.const 1)))
        (br $sum_z)))
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
