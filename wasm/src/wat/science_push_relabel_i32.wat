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
    (local $excessbase i32)
    (local $heightbase i32)
    (local $sweep i32)
    (local $i i32)
    (local $ex i32)
    (local $h i32)
    (local $nb i32)
    (local $nh i32)
    (local $minh i32)
    (local $cap i32)
    (local $pushed i32)
    (local $total i64)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $excessbase (i32.const 1024))
    (local.set $heightbase (i32.const 2048))

    (local.set $i (i32.const 0))
    (block $init_done
      (loop $init
        (br_if $init_done (i32.ge_u (local.get $i) (i32.const 128)))
        (i32.store (i32.add (local.get $excessbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
        (i32.store (i32.add (local.get $heightbase) (i32.shl (local.get $i) (i32.const 2))) (i32.const 0))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $init)))
    (i32.store (i32.add (local.get $heightbase) (i32.const 0)) (i32.const 128))
    (local.set $i (i32.const 1))
    (block $seed_done
      (loop $seed
        (br_if $seed_done (i32.ge_u (local.get $i) (i32.const 128)))
        (i32.store
          (i32.add (local.get $excessbase) (i32.shl (local.get $i) (i32.const 2)))
          (i32.add
            (i32.const 8)
            (i32.and
              (i32.add (i32.mul (local.get $i) (i32.const 7)) (i32.const 3))
              (i32.const 15))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $seed)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $sweep (i32.const 0))
    (block $sweep_done
      (loop $sweep_loop
        (br_if $sweep_done (i32.ge_u (local.get $sweep) (i32.const 32)))
        (local.set $i (i32.const 1))
        (block $node_done
          (loop $node_loop
            (br_if $node_done (i32.ge_u (local.get $i) (i32.const 127)))
            (local.set $ex
              (i32.load (i32.add (local.get $excessbase) (i32.shl (local.get $i) (i32.const 2)))))
            (if (i32.gt_s (local.get $ex) (i32.const 0))
              (then
                (local.set $h
                  (i32.load (i32.add (local.get $heightbase) (i32.shl (local.get $i) (i32.const 2)))))
                (local.set $minh (i32.const 100000))

                (local.set $nb (i32.and (i32.add (local.get $i) (i32.const 1)) (i32.const 127)))
                (local.set $nh
                  (i32.load (i32.add (local.get $heightbase) (i32.shl (local.get $nb) (i32.const 2)))))
                (if (i32.lt_s (local.get $nh) (local.get $minh))
                  (then (local.set $minh (local.get $nh))))
                (if
                  (i32.and
                    (i32.gt_s (local.get $ex) (i32.const 0))
                    (i32.eq (local.get $h) (i32.add (local.get $nh) (i32.const 1))))
                  (then
                    (local.set $cap
                      (i32.add
                        (i32.const 1)
                        (i32.and
                          (i32.add
                            (i32.add (i32.mul (local.get $i) (i32.const 3)) (local.get $sweep))
                            (i32.const 5))
                          (i32.const 7))))
                    (local.set $pushed
                      (if (result i32) (i32.lt_s (local.get $ex) (local.get $cap))
                        (then (local.get $ex))
                        (else (local.get $cap))))
                    (local.set $ex (i32.sub (local.get $ex) (local.get $pushed)))
                    (i32.store
                      (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2)))
                      (i32.add
                        (i32.load (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2))))
                        (local.get $pushed)))))

                (local.set $nb
                  (if (result i32) (i32.eqz (local.get $i))
                    (then (i32.const 127))
                    (else (i32.sub (local.get $i) (i32.const 1)))))
                (local.set $nh
                  (i32.load (i32.add (local.get $heightbase) (i32.shl (local.get $nb) (i32.const 2)))))
                (if (i32.lt_s (local.get $nh) (local.get $minh))
                  (then (local.set $minh (local.get $nh))))
                (if
                  (i32.and
                    (i32.gt_s (local.get $ex) (i32.const 0))
                    (i32.eq (local.get $h) (i32.add (local.get $nh) (i32.const 1))))
                  (then
                    (local.set $cap
                      (i32.add
                        (i32.const 1)
                        (i32.and
                          (i32.add
                            (i32.add (i32.mul (local.get $i) (i32.const 5)) (local.get $sweep))
                            (i32.const 3))
                          (i32.const 7))))
                    (local.set $pushed
                      (if (result i32) (i32.lt_s (local.get $ex) (local.get $cap))
                        (then (local.get $ex))
                        (else (local.get $cap))))
                    (local.set $ex (i32.sub (local.get $ex) (local.get $pushed)))
                    (i32.store
                      (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2)))
                      (i32.add
                        (i32.load (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2))))
                        (local.get $pushed)))))

                (local.set $nb
                  (i32.and
                    (i32.add (i32.mul (local.get $i) (i32.const 3)) (i32.add (local.get $sweep) (i32.const 7)))
                    (i32.const 127)))
                (local.set $nh
                  (i32.load (i32.add (local.get $heightbase) (i32.shl (local.get $nb) (i32.const 2)))))
                (if (i32.lt_s (local.get $nh) (local.get $minh))
                  (then (local.set $minh (local.get $nh))))
                (if
                  (i32.and
                    (i32.gt_s (local.get $ex) (i32.const 0))
                    (i32.eq (local.get $h) (i32.add (local.get $nh) (i32.const 1))))
                  (then
                    (local.set $cap
                      (i32.add
                        (i32.const 1)
                        (i32.and
                          (i32.add
                            (i32.add (i32.mul (local.get $i) (i32.const 7)) (local.get $sweep))
                            (i32.const 1))
                          (i32.const 7))))
                    (local.set $pushed
                      (if (result i32) (i32.lt_s (local.get $ex) (local.get $cap))
                        (then (local.get $ex))
                        (else (local.get $cap))))
                    (local.set $ex (i32.sub (local.get $ex) (local.get $pushed)))
                    (i32.store
                      (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2)))
                      (i32.add
                        (i32.load (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2))))
                        (local.get $pushed)))))

                (local.set $nb
                  (i32.and
                    (i32.add (i32.mul (local.get $i) (i32.const 5)) (i32.add (local.get $sweep) (i32.const 11)))
                    (i32.const 127)))
                (local.set $nh
                  (i32.load (i32.add (local.get $heightbase) (i32.shl (local.get $nb) (i32.const 2)))))
                (if (i32.lt_s (local.get $nh) (local.get $minh))
                  (then (local.set $minh (local.get $nh))))
                (if
                  (i32.and
                    (i32.gt_s (local.get $ex) (i32.const 0))
                    (i32.eq (local.get $h) (i32.add (local.get $nh) (i32.const 1))))
                  (then
                    (local.set $cap
                      (i32.add
                        (i32.const 1)
                        (i32.and
                          (i32.add
                            (i32.add (i32.mul (local.get $i) (i32.const 11)) (local.get $sweep))
                            (i32.const 9))
                          (i32.const 7))))
                    (local.set $pushed
                      (if (result i32) (i32.lt_s (local.get $ex) (local.get $cap))
                        (then (local.get $ex))
                        (else (local.get $cap))))
                    (local.set $ex (i32.sub (local.get $ex) (local.get $pushed)))
                    (i32.store
                      (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2)))
                      (i32.add
                        (i32.load (i32.add (local.get $excessbase) (i32.shl (local.get $nb) (i32.const 2))))
                        (local.get $pushed)))))

                (i32.store
                  (i32.add (local.get $excessbase) (i32.shl (local.get $i) (i32.const 2)))
                  (local.get $ex))
                (if (i32.gt_s (local.get $ex) (i32.const 0))
                  (then
                    (i32.store
                      (i32.add (local.get $heightbase) (i32.shl (local.get $i) (i32.const 2)))
                      (i32.add (local.get $minh) (i32.const 1)))))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $node_loop)))
        (local.set $sweep (i32.add (local.get $sweep) (i32.const 1)))
        (br $sweep_loop)))

    (local.set $total (i64.const 0))
    (local.set $i (i32.const 0))
    (block $sum_done
      (loop $sum_loop
        (br_if $sum_done (i32.ge_u (local.get $i) (i32.const 128)))
        (local.set $total
          (i64.add
            (local.get $total)
            (i64.extend_i32_s
              (i32.add
                (i32.load (i32.add (local.get $excessbase) (i32.shl (local.get $i) (i32.const 2))))
                (i32.load (i32.add (local.get $heightbase) (i32.shl (local.get $i) (i32.const 2))))))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $sum_loop)))
    (i64.store (i32.const 64) (local.get $total))

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
