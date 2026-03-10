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

  (func (export "_start")
    (local $base i32)
    (local $dist i32)
    (local $rep i32)
    (local $i i32)
    (local $j i32)
    (local $k i32)
    (local $ptr i32)
    (local $val i32)
    (local $cand i32)
    (local $dik i32)
    (local $sum i64)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $base (i32.const 1024))
    (local.set $dist (i32.const 20000))

    (local.set $i (i32.const 0))
    (block $init_i_done
      (loop $init_i
        (br_if $init_i_done (i32.ge_u (local.get $i) (i32.const 64)))
        (local.set $j (i32.const 0))
        (block $init_j_done
          (loop $init_j
            (br_if $init_j_done (i32.ge_u (local.get $j) (i32.const 64)))
            (local.set $ptr
              (i32.add
                (local.get $base)
                (i32.shl
                  (i32.add
                    (i32.mul (local.get $i) (i32.const 64))
                    (local.get $j))
                  (i32.const 2))))
            (local.set $val
              (if (result i32) (i32.eq (local.get $i) (local.get $j))
                (then (i32.const 0))
                (else
                  (if (result i32)
                    (i32.or
                      (i32.eq (i32.and (i32.add (local.get $i) (i32.const 1)) (i32.const 63)) (local.get $j))
                      (i32.eq (i32.and (i32.add (local.get $j) (i32.const 1)) (i32.const 63)) (local.get $i)))
                    (then
                      (i32.add
                        (i32.const 3)
                        (i32.and (i32.add (local.get $i) (local.get $j)) (i32.const 3))))
                    (else
                      (if (result i32)
                        (i32.eq
                          (i32.and
                            (i32.add
                              (i32.mul (local.get $i) (i32.const 7))
                              (i32.mul (local.get $j) (i32.const 11)))
                            (i32.const 15))
                          (i32.const 0))
                        (then
                          (i32.add
                            (i32.const 8)
                            (i32.and (i32.add (local.get $i) (local.get $j)) (i32.const 7))))
                        (else (i32.const 1000000))))))))
            (i32.store (local.get $ptr) (local.get $val))
            (local.set $j (i32.add (local.get $j) (i32.const 1)))
            (br $init_j)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $init_i)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $sum (i64.const 0))
    (local.set $rep (i32.const 0))
    (block $rep_done
      (loop $rep_loop
        (br_if $rep_done (i32.ge_u (local.get $rep) (i32.const 20)))

        (local.set $i (i32.const 0))
        (block $copy_done
          (loop $copy_loop
            (br_if $copy_done (i32.ge_u (local.get $i) (i32.const 4096)))
            (local.set $ptr (i32.shl (local.get $i) (i32.const 2)))
            (i32.store
              (i32.add (local.get $dist) (local.get $ptr))
              (i32.load (i32.add (local.get $base) (local.get $ptr))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $copy_loop)))

        (local.set $k (i32.const 0))
        (block $k_done
          (loop $k_loop
            (br_if $k_done (i32.ge_u (local.get $k) (i32.const 64)))
            (local.set $i (i32.const 0))
            (block $i_done
              (loop $i_loop
                (br_if $i_done (i32.ge_u (local.get $i) (i32.const 64)))
                (local.set $dik
                  (i32.load
                    (i32.add
                      (local.get $dist)
                      (i32.shl
                        (i32.add
                          (i32.mul (local.get $i) (i32.const 64))
                          (local.get $k))
                        (i32.const 2)))))
                (if (i32.lt_s (local.get $dik) (i32.const 1000000))
                  (then
                    (local.set $j (i32.const 0))
                    (block $j_done
                      (loop $j_loop
                        (br_if $j_done (i32.ge_u (local.get $j) (i32.const 64)))
                        (local.set $val
                          (i32.load
                            (i32.add
                              (local.get $dist)
                              (i32.shl
                                (i32.add
                                  (i32.mul (local.get $k) (i32.const 64))
                                  (local.get $j))
                                (i32.const 2)))))
                        (if (i32.lt_s (local.get $val) (i32.const 1000000))
                          (then
                            (local.set $cand (i32.add (local.get $dik) (local.get $val)))
                            (local.set $ptr
                              (i32.add
                                (local.get $dist)
                                (i32.shl
                                  (i32.add
                                    (i32.mul (local.get $i) (i32.const 64))
                                    (local.get $j))
                                  (i32.const 2))))
                            (if (i32.lt_s (local.get $cand) (i32.load (local.get $ptr)))
                              (then
                                (i32.store (local.get $ptr) (local.get $cand))))))
                        (local.set $j (i32.add (local.get $j) (i32.const 1)))
                        (br $j_loop)))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $i_loop)))
            (local.set $k (i32.add (local.get $k) (i32.const 1)))
            (br $k_loop)))

        (local.set $i (i32.const 0))
        (block $sum_rep_done
          (loop $sum_rep
            (br_if $sum_rep_done (i32.ge_u (local.get $i) (i32.const 64)))
            (local.set $j (i32.const 0))
            (block $sum_j_done
              (loop $sum_j
                (br_if $sum_j_done (i32.ge_u (local.get $j) (i32.const 64)))
                (if
                  (i32.eq
                    (i32.and (i32.add (local.get $i) (i32.mul (local.get $j) (i32.const 3))) (i32.const 7))
                    (i32.const 0))
                  (then
                    (local.set $sum
                      (i64.add
                        (local.get $sum)
                        (i64.extend_i32_s
                          (i32.load
                            (i32.add
                              (local.get $dist)
                              (i32.shl
                                (i32.add
                                  (i32.mul (local.get $i) (i32.const 64))
                                  (local.get $j))
                                (i32.const 2)))))))))
                (local.set $j (i32.add (local.get $j) (i32.const 1)))
                (br $sum_j)))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $sum_rep)))

        (local.set $rep (i32.add (local.get $rep) (i32.const 1)))
        (br $rep_loop)))
    (i64.store (i32.const 64) (local.get $sum))

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
