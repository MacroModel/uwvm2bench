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

  (func $pressure (param $rho f32) (param $mom f32) (param $ene f32) (result f32)
    (local $u f32)
    (local.set $u (f32.div (local.get $mom) (local.get $rho)))
    (f32.mul
      (f32.const 0.4)
      (f32.sub
        (local.get $ene)
        (f32.mul
          (f32.const 0.5)
          (f32.mul (local.get $mom) (local.get $u))))))

  (func $maxf (param $a f32) (param $b f32) (result f32)
    (if (result f32) (f32.gt (local.get $a) (local.get $b))
      (then (local.get $a))
      (else (local.get $b))))

  (func (export "_start")
    (local $rbase i32)
    (local $mbase i32)
    (local $ebase i32)
    (local $nrbase i32)
    (local $nmbase i32)
    (local $nebase i32)
    (local $step i32)
    (local $i i32)
    (local $im i32)
    (local $ip i32)
    (local $rl f32)
    (local $ml f32)
    (local $el f32)
    (local $rc f32)
    (local $mc f32)
    (local $ec f32)
    (local $rr f32)
    (local $mr f32)
    (local $er f32)
    (local $ul f32)
    (local $uc f32)
    (local $ur f32)
    (local $pl f32)
    (local $pc f32)
    (local $pr f32)
    (local $cl f32)
    (local $cc f32)
    (local $cr f32)
    (local $al f32)
    (local $ar f32)
    (local $fr_r f32)
    (local $fr_m f32)
    (local $fr_e f32)
    (local $fl_r f32)
    (local $fl_m f32)
    (local $fl_e f32)
    (local $sum f32)
    (local $t0 i64)
    (local $t1 i64)
    (local $diff i64)
    (local $ms_int i64)
    (local $ms_frac i32)
    (local $p i32)
    (local $nlen i32)

    (local.set $rbase (i32.const 1024))
    (local.set $mbase (i32.const 2048))
    (local.set $ebase (i32.const 3072))
    (local.set $nrbase (i32.const 4096))
    (local.set $nmbase (i32.const 5120))
    (local.set $nebase (i32.const 6144))

    (local.set $i (i32.const 0))
    (block $init_done
      (loop $init
        (br_if $init_done (i32.ge_u (local.get $i) (i32.const 192)))
        (if (i32.lt_u (local.get $i) (i32.const 96))
          (then
            (f32.store (i32.add (local.get $rbase) (i32.shl (local.get $i) (i32.const 2))) (f32.const 1.0))
            (f32.store (i32.add (local.get $mbase) (i32.shl (local.get $i) (i32.const 2))) (f32.const 0.35))
            (f32.store (i32.add (local.get $ebase) (i32.shl (local.get $i) (i32.const 2))) (f32.const 2.8)))
          (else
            (f32.store (i32.add (local.get $rbase) (i32.shl (local.get $i) (i32.const 2))) (f32.const 0.42))
            (f32.store (i32.add (local.get $mbase) (i32.shl (local.get $i) (i32.const 2))) (f32.const -0.18))
            (f32.store (i32.add (local.get $ebase) (i32.shl (local.get $i) (i32.const 2))) (f32.const 1.3))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $init)))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 16))
    drop
    (local.set $t0 (i64.load (i32.const 16)))

    (local.set $step (i32.const 0))
    (block $step_done
      (loop $step_loop
        (br_if $step_done (i32.ge_u (local.get $step) (i32.const 96)))
        (local.set $i (i32.const 0))
        (block $cell_done
          (loop $cell_loop
            (br_if $cell_done (i32.ge_u (local.get $i) (i32.const 192)))
            (local.set $im
              (if (result i32) (i32.eqz (local.get $i))
                (then (i32.const 191))
                (else (i32.sub (local.get $i) (i32.const 1)))))
            (local.set $ip
              (if (result i32) (i32.eq (local.get $i) (i32.const 191))
                (then (i32.const 0))
                (else (i32.add (local.get $i) (i32.const 1)))))

            (local.set $rl (f32.load (i32.add (local.get $rbase) (i32.shl (local.get $im) (i32.const 2)))))
            (local.set $ml (f32.load (i32.add (local.get $mbase) (i32.shl (local.get $im) (i32.const 2)))))
            (local.set $el (f32.load (i32.add (local.get $ebase) (i32.shl (local.get $im) (i32.const 2)))))
            (local.set $rc (f32.load (i32.add (local.get $rbase) (i32.shl (local.get $i) (i32.const 2)))))
            (local.set $mc (f32.load (i32.add (local.get $mbase) (i32.shl (local.get $i) (i32.const 2)))))
            (local.set $ec (f32.load (i32.add (local.get $ebase) (i32.shl (local.get $i) (i32.const 2)))))
            (local.set $rr (f32.load (i32.add (local.get $rbase) (i32.shl (local.get $ip) (i32.const 2)))))
            (local.set $mr (f32.load (i32.add (local.get $mbase) (i32.shl (local.get $ip) (i32.const 2)))))
            (local.set $er (f32.load (i32.add (local.get $ebase) (i32.shl (local.get $ip) (i32.const 2)))))

            (local.set $ul (f32.div (local.get $ml) (local.get $rl)))
            (local.set $uc (f32.div (local.get $mc) (local.get $rc)))
            (local.set $ur (f32.div (local.get $mr) (local.get $rr)))
            (local.set $pl (call $pressure (local.get $rl) (local.get $ml) (local.get $el)))
            (local.set $pc (call $pressure (local.get $rc) (local.get $mc) (local.get $ec)))
            (local.set $pr (call $pressure (local.get $rr) (local.get $mr) (local.get $er)))
            (local.set $cl (f32.sqrt (f32.div (f32.mul (f32.const 1.4) (local.get $pl)) (local.get $rl))))
            (local.set $cc (f32.sqrt (f32.div (f32.mul (f32.const 1.4) (local.get $pc)) (local.get $rc))))
            (local.set $cr (f32.sqrt (f32.div (f32.mul (f32.const 1.4) (local.get $pr)) (local.get $rr))))

            (local.set $al
              (call $maxf
                (f32.add (f32.abs (local.get $ul)) (local.get $cl))
                (f32.add (f32.abs (local.get $uc)) (local.get $cc))))
            (local.set $ar
              (call $maxf
                (f32.add (f32.abs (local.get $uc)) (local.get $cc))
                (f32.add (f32.abs (local.get $ur)) (local.get $cr))))

            (local.set $fl_r
              (f32.sub
                (f32.mul (f32.const 0.5) (f32.add (local.get $ml) (local.get $mc)))
                (f32.mul (f32.const 0.5) (f32.mul (local.get $al) (f32.sub (local.get $rc) (local.get $rl))))))
            (local.set $fl_m
              (f32.sub
                (f32.mul
                  (f32.const 0.5)
                  (f32.add
                    (f32.add (f32.mul (local.get $ml) (local.get $ul)) (local.get $pl))
                    (f32.add (f32.mul (local.get $mc) (local.get $uc)) (local.get $pc))))
                (f32.mul (f32.const 0.5) (f32.mul (local.get $al) (f32.sub (local.get $mc) (local.get $ml))))))
            (local.set $fl_e
              (f32.sub
                (f32.mul
                  (f32.const 0.5)
                  (f32.add
                    (f32.mul (f32.add (local.get $el) (local.get $pl)) (local.get $ul))
                    (f32.mul (f32.add (local.get $ec) (local.get $pc)) (local.get $uc))))
                (f32.mul (f32.const 0.5) (f32.mul (local.get $al) (f32.sub (local.get $ec) (local.get $el))))))

            (local.set $fr_r
              (f32.sub
                (f32.mul (f32.const 0.5) (f32.add (local.get $mc) (local.get $mr)))
                (f32.mul (f32.const 0.5) (f32.mul (local.get $ar) (f32.sub (local.get $rr) (local.get $rc))))))
            (local.set $fr_m
              (f32.sub
                (f32.mul
                  (f32.const 0.5)
                  (f32.add
                    (f32.add (f32.mul (local.get $mc) (local.get $uc)) (local.get $pc))
                    (f32.add (f32.mul (local.get $mr) (local.get $ur)) (local.get $pr))))
                (f32.mul (f32.const 0.5) (f32.mul (local.get $ar) (f32.sub (local.get $mr) (local.get $mc))))))
            (local.set $fr_e
              (f32.sub
                (f32.mul
                  (f32.const 0.5)
                  (f32.add
                    (f32.mul (f32.add (local.get $ec) (local.get $pc)) (local.get $uc))
                    (f32.mul (f32.add (local.get $er) (local.get $pr)) (local.get $ur))))
                (f32.mul (f32.const 0.5) (f32.mul (local.get $ar) (f32.sub (local.get $er) (local.get $ec))))))

            (f32.store
              (i32.add (local.get $nrbase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.max
                (f32.const 0.30)
                (f32.sub (local.get $rc) (f32.mul (f32.const 0.12) (f32.sub (local.get $fr_r) (local.get $fl_r))))))
            (f32.store
              (i32.add (local.get $nmbase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.sub (local.get $mc) (f32.mul (f32.const 0.12) (f32.sub (local.get $fr_m) (local.get $fl_m)))))
            (f32.store
              (i32.add (local.get $nebase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.max
                (f32.const 0.80)
                (f32.sub (local.get $ec) (f32.mul (f32.const 0.12) (f32.sub (local.get $fr_e) (local.get $fl_e))))))

            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $cell_loop)))

        (local.set $i (i32.const 0))
        (block $copy_done
          (loop $copy
            (br_if $copy_done (i32.ge_u (local.get $i) (i32.const 192)))
            (f32.store (i32.add (local.get $rbase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.load (i32.add (local.get $nrbase) (i32.shl (local.get $i) (i32.const 2)))))
            (f32.store (i32.add (local.get $mbase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.load (i32.add (local.get $nmbase) (i32.shl (local.get $i) (i32.const 2)))))
            (f32.store (i32.add (local.get $ebase) (i32.shl (local.get $i) (i32.const 2)))
              (f32.load (i32.add (local.get $nebase) (i32.shl (local.get $i) (i32.const 2)))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br $copy)))
        (local.set $step (i32.add (local.get $step) (i32.const 1)))
        (br $step_loop)))

    (local.set $sum (f32.const 0.0))
    (local.set $i (i32.const 0))
    (block $sum_done
      (loop $sum_loop
        (br_if $sum_done (i32.ge_u (local.get $i) (i32.const 192)))
        (local.set $sum
          (f32.add
            (local.get $sum)
            (f32.add
              (f32.load (i32.add (local.get $rbase) (i32.shl (local.get $i) (i32.const 2))))
              (f32.load (i32.add (local.get $ebase) (i32.shl (local.get $i) (i32.const 2)))))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $sum_loop)))
    (f32.store (i32.const 64) (local.get $sum))

    (call $clock_time_get (i32.const 1) (i64.const 0) (i32.const 24))
    drop
    (local.set $t1 (i64.load (i32.const 24)))
    (local.set $diff (i64.sub (local.get $t1) (local.get $t0)))
    (local.set $ms_int (i64.div_u (local.get $diff) (i64.const 1000000)))
    (local.set $ms_frac (i32.wrap_i64 (i64.div_u (i64.rem_u (local.get $diff) (i64.const 1000000)) (i64.const 1000))))

    (local.set $p (i32.const 7168))
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
