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

  (func $transition (param $i i32) (param $j i32) (param $t i32) (result f32)
    (local $d i32)
    (local.set $d
      (if (result i32) (i32.gt_u (local.get $i) (local.get $j))
        (then (i32.sub (local.get $i) (local.get $j)))
        (else (i32.sub (local.get $j) (local.get $i)))))
    (f32.div
      (f32.const 1.0)
      (f32.add
        (f32.const 1.0)
        (f32.add
          (f32.mul (f32.convert_i32_u (local.get $d)) (f32.const 0.14))
          (f32.mul
            (f32.convert_i32_u
              (i32.and
                (i32.add
                  (i32.add
                    (i32.mul (local.get $i) (i32.const 3))
                    (i32.mul (local.get $j) (i32.const 5)))
                  (local.get $t))
                (i32.const 7)))
            (f32.const 0.02))))))

  (func $emission (param $j i32) (param $t i32) (result f32)
    (local $obs i32)
    (local $sym i32)
    (local $d i32)
    (local.set $obs (i32.and (i32.add (i32.mul (local.get $t) (i32.const 7)) (i32.const 3)) (i32.const 15)))
    (local.set $sym (i32.and (i32.add (i32.mul (local.get $j) (i32.const 5)) (i32.const 1)) (i32.const 15)))
    (local.set $d
      (if (result i32) (i32.gt_u (local.get $obs) (local.get $sym))
        (then (i32.sub (local.get $obs) (local.get $sym)))
        (else (i32.sub (local.get $sym) (local.get $obs)))))
    (f32.div
      (f32.const 1.0)
      (f32.add (f32.const 1.0) (f32.mul (f32.convert_i32_u (local.get $d)) (f32.const 0.20)))))

  (func (export "_start")
    (local $abase i32)
    (local $nbase i32)
    (local $bbase i32)
    (local $t i32)
    (local $i i32)
    (local $j i32)
    (local $sumv f32)
    (local $total f32)
    (local $tmp f32)
    (local $score f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $abase (i32.const 1024))
    (local.set $nbase (i32.const 2048))
    (local.set $bbase (i32.const 3072))

    (local.set $i (i32.const 0))
    (block $init_done
      (loop $init
        (br_if $init_done (i32.ge_u (local.get $i) (i32.const 48)))
        (f32.store
          (i32.add (local.get $abase) (i32.shl (local.get $i) (i32.const 2)))
          (f32.const 0.020833334))
        (f32.store
          (i32.add (local.get $bbase) (i32.shl (local.get $i) (i32.const 2)))
          (f32.const 1.0))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $init)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $score (f32.const 0.0))
    (local.set $t (i32.const 0))
    (block $fwd_done
      (loop $fwd_loop
        (br_if $fwd_done (i32.ge_u (local.get $t) (i32.const 1536)))
        (local.set $total (f32.const 0.0))
        (local.set $j (i32.const 0))
        (block $j_done
          (loop $j_loop
            (br_if $j_done (i32.ge_u (local.get $j) (i32.const 48)))
            (local.set $sumv (f32.const 0.0))
            (local.set $i (i32.const 0))
            (block $i_done
              (loop $i_loop
                (br_if $i_done (i32.ge_u (local.get $i) (i32.const 48)))
                (local.set $sumv
                  (f32.add
                    (local.get $sumv)
                    (f32.mul
                      (f32.load (i32.add (local.get $abase) (i32.shl (local.get $i) (i32.const 2))))
                      (call $transition (local.get $i) (local.get $j) (local.get $t)))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $i_loop)))
            (local.set $tmp (f32.mul (local.get $sumv) (call $emission (local.get $j) (local.get $t))))
            (f32.store (i32.add (local.get $nbase) (i32.shl (local.get $j) (i32.const 2))) (local.get $tmp))
            (local.set $total (f32.add (local.get $total) (local.get $tmp)))
            (local.set $j (i32.add (local.get $j) (i32.const 1)))
            (br $j_loop)))
        (local.set $i (i32.const 0))
        (block $norm_done
          (loop $norm
            (br_if $norm_done (i32.ge_u (local.get $i) (i32.const 48)))
            (f32.store
              (i32.add (local.get $abase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.div
                (f32.load (i32.add (local.get $nbase) (i32.shl (local.get $i) (i32.const 2))))
                (f32.add (local.get $total) (f32.const 0.000001))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $norm)))
        (local.set $score (f32.add (local.get $score) (local.get $total)))
        (local.set $t (i32.add (local.get $t) (i32.const 1)))
        (br $fwd_loop)))

    (local.set $t (i32.const 1535))
    (block $bwd_done
      (loop $bwd_loop
        (local.set $total (f32.const 0.0))
        (local.set $i (i32.const 0))
        (block $bi_done
          (loop $bi_loop
            (br_if $bi_done (i32.ge_u (local.get $i) (i32.const 48)))
            (local.set $sumv (f32.const 0.0))
            (local.set $j (i32.const 0))
            (block $bj_done
              (loop $bj_loop
                (br_if $bj_done (i32.ge_u (local.get $j) (i32.const 48)))
                (local.set $sumv
                  (f32.add
                    (local.get $sumv)
                    (f32.mul
                      (call $transition (local.get $i) (local.get $j) (local.get $t))
                      (f32.mul
                        (call $emission (local.get $j) (local.get $t))
                        (f32.load (i32.add (local.get $bbase) (i32.shl (local.get $j) (i32.const 2))))))))
                (local.set $j (i32.add (local.get $j) (i32.const 1)))
                (br $bj_loop)))
            (f32.store (i32.add (local.get $nbase) (i32.shl (local.get $i) (i32.const 2))) (local.get $sumv))
            (local.set $total (f32.add (local.get $total) (local.get $sumv)))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $bi_loop)))
        (local.set $i (i32.const 0))
        (block $bnorm_done
          (loop $bnorm
            (br_if $bnorm_done (i32.ge_u (local.get $i) (i32.const 48)))
            (f32.store
              (i32.add (local.get $bbase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.div
                (f32.load (i32.add (local.get $nbase) (i32.shl (local.get $i) (i32.const 2))))
                (f32.add (local.get $total) (f32.const 0.000001))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $bnorm)))
        (local.set $score (f32.add (local.get $score) (local.get $total)))
        (br_if $bwd_done (i32.eqz (local.get $t)))
        (local.set $t (i32.sub (local.get $t) (i32.const 1)))
        (br $bwd_loop)))

    (f32.store (i32.const 64) (local.get $score))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 4096))
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
