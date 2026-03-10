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

  (func $iabs (param $x i32) (result i32)
    (if (result i32) (i32.lt_s (local.get $x) (i32.const 0))
      (then (i32.sub (i32.const 0) (local.get $x)))
      (else (local.get $x))))

  (func (export "_start")
    (local $curbase i32)
    (local $nextbase i32)
    (local $iter i32)
    (local $i i32)
    (local $s i32)
    (local $sp i32)
    (local $prev i32)
    (local $next i32)
    (local $unary i32)
    (local $compat i32)
    (local $cand i32)
    (local $best1 i32)
    (local $best2 i32)
    (local $bestself i32)
    (local $score i32)
    (local $off i32)
    (local $sum i64)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $curbase (i32.const 1024))
    (local.set $nextbase (i32.const 3072))

    (local.set $i (i32.const 0))
    (block $zero_done
      (loop $zero
        (br_if $zero_done (i32.ge_u (local.get $i) (i32.const 384)))
        (i32.store (i32.add (local.get $curbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
        (i32.store (i32.add (local.get $nextbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $zero)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $iter (i32.const 0))
    (block $iter_done
      (loop $iter_loop
        (br_if $iter_done (i32.ge_u (local.get $iter) (i32.const 96)))
        (local.set $i (i32.const 0))
        (block $node_done
          (loop $node_loop
            (br_if $node_done (i32.ge_u (local.get $i) (i32.const 96)))
            (local.set $prev
              (if (result i32) (i32.eqz (local.get $i))
                (then (i32.const 95))
                (else (i32.sub (local.get $i) (i32.const 1)))))
            (local.set $next
              (if (result i32) (i32.eq (local.get $i) (i32.const 95))
                (then (i32.const 0))
                (else (i32.add (local.get $i) (i32.const 1)))))
            (local.set $s (i32.const 0))
            (block $state_done
              (loop $state_loop
                (br_if $state_done (i32.ge_u (local.get $s) (i32.const 4)))
                (local.set $best1 (i32.const 1000000))
                (local.set $best2 (i32.const 1000000))
                (local.set $sp (i32.const 0))
                (block $prev_done
                  (loop $prev_loop
                    (br_if $prev_done (i32.ge_u (local.get $sp) (i32.const 4)))
                    (local.set $compat
                      (i32.add
                        (call $iabs (i32.sub (local.get $s) (local.get $sp)))
                        (i32.and (i32.add (local.get $i) (i32.add (local.get $sp) (local.get $iter))) (i32.const 1))))
                    (local.set $cand
                      (i32.add
                        (local.get $compat)
                        (i32.load
                          (i32.add
                            (local.get $curbase)
                            (i32.shl (i32.add (i32.mul (local.get $prev) (i32.const 4)) (local.get $sp)) (i32.const 2)))))
                    (if (i32.lt_s (local.get $cand) (local.get $best1))
                      (then (local.set $best1 (local.get $cand))))
                    (local.set $sp (i32.add (local.get $sp) (i32.const 1)))
                    (br $prev_loop)))
                (local.set $sp (i32.const 0))
                (block $next_done
                  (loop $next_loop
                    (br_if $next_done (i32.ge_u (local.get $sp) (i32.const 4)))
                    (local.set $compat
                      (i32.add
                        (call $iabs (i32.sub (local.get $s) (local.get $sp)))
                        (i32.and (i32.add (local.get $i) (i32.add (i32.mul (local.get $sp) (i32.const 3)) (local.get $iter))) (i32.const 1))))
                    (local.set $cand
                      (i32.add
                        (local.get $compat)
                        (i32.load
                          (i32.add
                            (local.get $curbase)
                            (i32.shl (i32.add (i32.mul (local.get $next) (i32.const 4)) (local.get $sp)) (i32.const 2)))))
                    (if (i32.lt_s (local.get $cand) (local.get $best2))
                      (then (local.set $best2 (local.get $cand))))
                    (local.set $sp (i32.add (local.get $sp) (i32.const 1)))
                    (br $next_loop)))
                (local.set $unary
                  (call $iabs
                    (i32.sub
                      (i32.and
                        (i32.add
                          (i32.add
                            (i32.mul (local.get $i) (i32.const 5))
                            (i32.mul (local.get $s) (i32.const 7)))
                          (local.get $iter))
                        (i32.const 15))
                      (i32.const 7))))
                (local.set $score
                  (i32.add (local.get $unary) (i32.add (local.get $best1) (local.get $best2))))
                (local.set $off
                  (i32.shl (i32.add (i32.mul (local.get $i) (i32.const 4)) (local.get $s)) (i32.const 2)))
                (i32.store (i32.add (local.get $nextbase) (local.get $off)) (local.get $score))
                (local.set $s (i32.add (local.get $s) (i32.const 1)))
                (br $state_loop)))

            (local.set $bestself (i32.const 1000000))
            (local.set $s (i32.const 0))
            (block $norm_find_done
              (loop $norm_find
                (br_if $norm_find_done (i32.ge_u (local.get $s) (i32.const 4)))
                (local.set $score
                  (i32.load
                    (i32.add
                      (local.get $nextbase)
                      (i32.shl (i32.add (i32.mul (local.get $i) (i32.const 4)) (local.get $s)) (i32.const 2)))))
                (if (i32.lt_s (local.get $score) (local.get $bestself))
                  (then (local.set $bestself (local.get $score))))
                (local.set $s (i32.add (local.get $s) (i32.const 1)))
                (br $norm_find)))
            (local.set $s (i32.const 0))
            (block $norm_done
              (loop $norm
                (br_if $norm_done (i32.ge_u (local.get $s) (i32.const 4)))
                (local.set $off
                  (i32.shl (i32.add (i32.mul (local.get $i) (i32.const 4)) (local.get $s)) (i32.const 2)))
                (i32.store
                  (i32.add (local.get $nextbase) (local.get $off))
                  (i32.sub
                    (i32.load (i32.add (local.get $nextbase) (local.get $off)))
                    (local.get $bestself)))
                (local.set $s (i32.add (local.get $s) (i32.const 1)))
                (br $norm)))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $node_loop)))

        (local.set $i (i32.const 0))
        (block $copy_done
          (loop $copy
            (br_if $copy_done (i32.ge_u (local.get $i) (i32.const 384)))
            (i32.store
              (i32.add (local.get $curbase) (i32.shl (local.get $i) (i32.const 2)))
              (i32.load (i32.add (local.get $nextbase) (i32.shl (local.get $i) (i32.const 2)))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $copy)))
        (local.set $iter (i32.add (local.get $iter) (i32.const 1)))
        (br $iter_loop)))

    (local.set $sum (i64.const 0))
    (local.set $i (i32.const 0))
    (block $sum_done
      (loop $sum_loop
        (br_if $sum_done (i32.ge_u (local.get $i) (i32.const 384)))
        (local.set $sum
          (i64.add
            (local.get $sum)
            (i64.extend_i32_s
              (i32.load (i32.add (local.get $curbase) (i32.shl (local.get $i) (i32.const 2)))))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $sum_loop)))
    (i64.store (i32.const 64) (local.get $sum))

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
    (call $proc_exit (i32.const 0))))
))
