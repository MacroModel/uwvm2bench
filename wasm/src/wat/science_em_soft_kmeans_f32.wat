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

  (func $sample_val (param $s i32) (param $d i32) (result f32)
    (f32.add
      (f32.mul
        (f32.convert_i32_u
          (i32.and
            (i32.add
              (i32.add
                (i32.mul (local.get $s) (i32.const 7))
                (i32.mul (local.get $d) (i32.const 13)))
              (i32.const 3))
            (i32.const 31)))
        (f32.const 0.03))
      (f32.mul
        (f32.convert_i32_u (i32.and (local.get $s) (i32.const 7)))
        (f32.const 0.07))))

  (func (export "_start")
    (local $cbase i32)
    (local $cntbase i32)
    (local $sumbase i32)
    (local $wbase i32)
    (local $iter i32)
    (local $k i32)
    (local $d i32)
    (local $s i32)
    (local $off i32)
    (local $score f32)
    (local $totalw f32)
    (local $dist f32)
    (local $sv f32)
    (local $cv f32)
    (local $w f32)
    (local $count f32)
    (local $sum f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $cbase (i32.const 1024))
    (local.set $cntbase (i32.const 2048))
    (local.set $sumbase (i32.const 3072))
    (local.set $wbase (i32.const 4096))

    (local.set $k (i32.const 0))
    (block $init_k_done
      (loop $init_k
        (br_if $init_k_done (i32.ge_u (local.get $k) (i32.const 8)))
        (f32.store (i32.add (local.get $cntbase) (i32.shl (local.get $k) (i32.const 2))) (f32.const 0.0))
        (local.set $d (i32.const 0))
        (block $init_d_done
          (loop $init_d
            (br_if $init_d_done (i32.ge_u (local.get $d) (i32.const 6)))
            (f32.store
              (i32.add
                (local.get $cbase)
                (i32.shl (i32.add (i32.mul (local.get $k) (i32.const 6)) (local.get $d)) (i32.const 2)))
              (f32.add
                (f32.mul (f32.convert_i32_u (local.get $k)) (f32.const 0.08))
                (f32.mul (f32.convert_i32_u (local.get $d)) (f32.const 0.05))))
            (local.set $d (i32.add (local.get $d) (i32.const 1)))
            (br $init_d)))
        (local.set $k (i32.add (local.get $k) (i32.const 1)))
        (br $init_k)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $iter (i32.const 0))
    (block $iter_done
      (loop $iter_loop
        (br_if $iter_done (i32.ge_u (local.get $iter) (i32.const 72)))
        (local.set $k (i32.const 0))
        (block $zero_k_done
          (loop $zero_k
            (br_if $zero_k_done (i32.ge_u (local.get $k) (i32.const 8)))
            (f32.store (i32.add (local.get $cntbase) (i32.shl (local.get $k) (i32.const 2))) (f32.const 0.0))
            (local.set $d (i32.const 0))
            (block $zero_d_done
              (loop $zero_d
                (br_if $zero_d_done (i32.ge_u (local.get $d) (i32.const 6)))
                (f32.store
                  (i32.add
                    (local.get $sumbase)
                    (i32.shl (i32.add (i32.mul (local.get $k) (i32.const 6)) (local.get $d)) (i32.const 2)))
                  (f32.const 0.0))
                (local.set $d (i32.add (local.get $d) (i32.const 1)))
                (br $zero_d)))
            (local.set $k (i32.add (local.get $k) (i32.const 1)))
            (br $zero_k)))

        (local.set $s (i32.const 0))
        (block $sample_done
          (loop $sample_loop
            (br_if $sample_done (i32.ge_u (local.get $s) (i32.const 256)))
            (local.set $totalw (f32.const 0.0))
            (local.set $k (i32.const 0))
            (block $score_done
              (loop $score_loop
                (br_if $score_done (i32.ge_u (local.get $k) (i32.const 8)))
                (local.set $dist (f32.const 0.0))
                (local.set $d (i32.const 0))
                (block $dist_done
                  (loop $dist_loop
                    (br_if $dist_done (i32.ge_u (local.get $d) (i32.const 6)))
                    (local.set $sv (call $sample_val (local.get $s) (local.get $d)))
                    (local.set $cv
                      (f32.load
                        (i32.add
                          (local.get $cbase)
                          (i32.shl (i32.add (i32.mul (local.get $k) (i32.const 6)) (local.get $d)) (i32.const 2)))))
                    (local.set $dist
                      (f32.add
                        (local.get $dist)
                        (f32.mul
                          (f32.sub (local.get $sv) (local.get $cv))
                          (f32.sub (local.get $sv) (local.get $cv)))))
                    (local.set $d (i32.add (local.get $d) (i32.const 1)))
                    (br $dist_loop)))
                (local.set $score (f32.div (f32.const 1.0) (f32.add (f32.const 1.0) (local.get $dist))))
                (f32.store (i32.add (local.get $wbase) (i32.shl (local.get $k) (i32.const 2))) (local.get $score))
                (local.set $totalw (f32.add (local.get $totalw) (local.get $score)))
                (local.set $k (i32.add (local.get $k) (i32.const 1)))
                (br $score_loop)))

            (local.set $k (i32.const 0))
            (block $acc_k_done
              (loop $acc_k
                (br_if $acc_k_done (i32.ge_u (local.get $k) (i32.const 8)))
                (local.set $w
                  (f32.div
                    (f32.load (i32.add (local.get $wbase) (i32.shl (local.get $k) (i32.const 2))))
                    (f32.add (local.get $totalw) (f32.const 0.0001))))
                (f32.store
                  (i32.add (local.get $cntbase) (i32.shl (local.get $k) (i32.const 2)))
                  (f32.add
                    (f32.load (i32.add (local.get $cntbase) (i32.shl (local.get $k) (i32.const 2))))
                    (local.get $w)))
                (local.set $d (i32.const 0))
                (block $acc_d_done
                  (loop $acc_d
                    (br_if $acc_d_done (i32.ge_u (local.get $d) (i32.const 6)))
                    (local.set $sv (call $sample_val (local.get $s) (local.get $d)))
                    (local.set $off
                      (i32.shl (i32.add (i32.mul (local.get $k) (i32.const 6)) (local.get $d)) (i32.const 2)))
                    (f32.store
                      (i32.add (local.get $sumbase) (local.get $off))
                      (f32.add
                        (f32.load (i32.add (local.get $sumbase) (local.get $off)))
                        (f32.mul (local.get $w) (local.get $sv))))
                    (local.set $d (i32.add (local.get $d) (i32.const 1)))
                    (br $acc_d)))
                (local.set $k (i32.add (local.get $k) (i32.const 1)))
                (br $acc_k)))
            (local.set $s (i32.add (local.get $s) (i32.const 1)))
            (br $sample_loop)))

        (local.set $k (i32.const 0))
        (block $upd_k_done
          (loop $upd_k
            (br_if $upd_k_done (i32.ge_u (local.get $k) (i32.const 8)))
            (local.set $count
              (f32.add
                (f32.load (i32.add (local.get $cntbase) (i32.shl (local.get $k) (i32.const 2))))
                (f32.const 0.001)))
            (local.set $d (i32.const 0))
            (block $upd_d_done
              (loop $upd_d
                (br_if $upd_d_done (i32.ge_u (local.get $d) (i32.const 6)))
                (local.set $off
                  (i32.shl (i32.add (i32.mul (local.get $k) (i32.const 6)) (local.get $d)) (i32.const 2)))
                (local.set $sum (f32.div (f32.load (i32.add (local.get $sumbase) (local.get $off))) (local.get $count)))
                (f32.store
                  (i32.add (local.get $cbase) (local.get $off))
                  (f32.add
                    (f32.mul (f32.const 0.90) (f32.load (i32.add (local.get $cbase) (local.get $off))))
                    (f32.mul (f32.const 0.10) (local.get $sum))))
                (local.set $d (i32.add (local.get $d) (i32.const 1)))
                (br $upd_d)))
            (local.set $k (i32.add (local.get $k) (i32.const 1)))
            (br $upd_k)))
        (local.set $iter (i32.add (local.get $iter) (i32.const 1)))
        (br $iter_loop)))

    (local.set $sum (f32.const 0.0))
    (local.set $k (i32.const 0))
    (block $sum_k_done
      (loop $sum_k
        (br_if $sum_k_done (i32.ge_u (local.get $k) (i32.const 8)))
        (local.set $d (i32.const 0))
        (block $sum_d_done
          (loop $sum_d
            (br_if $sum_d_done (i32.ge_u (local.get $d) (i32.const 6)))
            (local.set $sum
              (f32.add
                (local.get $sum)
                (f32.load
                  (i32.add
                    (local.get $cbase)
                    (i32.shl (i32.add (i32.mul (local.get $k) (i32.const 6)) (local.get $d)) (i32.const 2))))))
            (local.set $d (i32.add (local.get $d) (i32.const 1)))
            (br $sum_d)))
        (local.set $k (i32.add (local.get $k) (i32.const 1)))
        (br $sum_k)))
    (f32.store (i32.const 64) (local.get $sum))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 5120))
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
