(module
(func $memset (type 8) (param i32 i32 i32) (result i32)
    (local i32 i32 i32)
    block  ;; label = @1
      local.get 2
      i32.eqz
      br_if 0 (;@1;)
      local.get 2
      i32.const 7
      i32.and
      local.set 4
      local.get 2
      i32.const 1
      i32.sub
      i32.const 7
      i32.ge_u
      if  ;; label = @2
        local.get 2
        i32.const -8
        i32.and
        local.set 5
        loop  ;; label = @3
          local.get 0
          local.get 3
          i32.add
          local.tee 2
          local.get 1
          i32.store8
          local.get 2
          i32.const 7
          i32.add
          local.get 1
          i32.store8
          local.get 2
          i32.const 6
          i32.add
          local.get 1
          i32.store8
          local.get 2
          i32.const 5
          i32.add
          local.get 1
          i32.store8
          local.get 2
          i32.const 4
          i32.add
          local.get 1
          i32.store8
          local.get 2
          i32.const 3
          i32.add
          local.get 1
          i32.store8
          local.get 2
          i32.const 2
          i32.add
          local.get 1
          i32.store8
          local.get 2
          i32.const 1
          i32.add
          local.get 1
          i32.store8
          local.get 5
          local.get 3
          i32.const 8
          i32.add
          local.tee 3
          i32.ne
          br_if 0 (;@3;)
        end
      end
      local.get 4
      i32.eqz
      br_if 0 (;@1;)
      local.get 0
      local.get 3
      i32.add
      local.set 2
      loop  ;; label = @2
        local.get 2
        local.get 1
        i32.store8
        local.get 2
        i32.const 1
        i32.add
        local.set 2
        local.get 4
        i32.const 1
        i32.sub
        local.tee 4
        br_if 0 (;@2;)
      end
    end
    local.get 0)
)
