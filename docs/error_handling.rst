Low-Level Error Handling & Branch Optimization
==============================================

.. note::
   This section describes the architectural decisions regarding function return
   types and binary layout optimization within the driver.

Architectural Overview
----------------------

Low latency workloads demand deterministic execution paths.
By utilizing the ``unlikely()`` macro and standardized error codes, we
enforce a strict separation between the hot path and cold path.
In future commits, an error path will be implemented for providing raw data on error state,
such as NIC register's state.
According to current plans, this implementation includes ;

- EEPROM state,
- Link State,
- Ring buffer State.

In or before implementation, this plans may be refactored.

Branch Prediction and Layout Optimization
-----------------------------------------

The driver leverages the unlikely macro to explicitly separate the hot path from
error handling logic. This provides several architectural advantages:
Instruction cache efficiency : Compiler pushes error return instructions
to the end of the binary, which provides reducing I-cache misses.
Branch merging: Multiple error branches jumps to same error code,which reduces
instruction footprint. This optimization doesn't provided by branch prediction logic,
but with the leverage of unlikely macros, it's increasing the leverage of it.
Pipeline prefetching: By keeping the happy path linear, we provided a wanted condition
for CPU prefetcher to subsequent instructions into the pipeline without interrupting with
branches.
Branch prediction is also mentioned in :ref:`happy_path_logic`,
Below title 'Happy path is fast, error path is not'.
Within the examination of a function with objdump, it can be proven.
Used command : 'objdump -d src/init.o'

.. code-block:: console

    src/init.o:     file format elf64-x86-64


    Disassembly of section .text:

    0000000000000000 <ixgbe_probe>:
       0:	48 8b 47 08          	mov    0x8(%rdi),%rax
       4:	c7 80 88 08 00 00 ff 	movl   $0x7fffffff,0x888(%rax)
       b:	ff ff 7f
       e:	8b 90 80 08 00 00    	mov    0x880(%rax),%edx
      14:	85 d2                	test   %edx,%edx
      16:	0f 85 fc 00 00 00    	jne    118 <ixgbe_probe+0x118>
      1c:	55                   	push   %rbp
      1d:	48 89 fd             	mov    %rdi,%rbp
      20:	bf 10 27 00 00       	mov    $0x2710,%edi
      25:	53                   	push   %rbx
      26:	bb 32 00 00 00       	mov    $0x32,%ebx
      2b:	48 83 ec 08          	sub    $0x8,%rsp
      2f:	8b 10                	mov    (%rax),%edx
      31:	81 ca 08 00 00 04    	or     $0x4000008,%edx
      37:	89 10                	mov    %edx,(%rax)
      39:	e8 00 00 00 00       	call   3e <ixgbe_probe+0x3e>
      3e:	48 8b 45 08          	mov    0x8(%rbp),%rax
      42:	8b 10                	mov    (%rax),%edx
      44:	81 e2 08 00 00 04    	and    $0x4000008,%edx
      4a:	74 23                	je     6f <ixgbe_probe+0x6f>
      4c:	e9 9f 00 00 00       	jmp    f0 <ixgbe_probe+0xf0>
      51:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
      58:	bf e8 03 00 00       	mov    $0x3e8,%edi
      5d:	e8 00 00 00 00       	call   62 <ixgbe_probe+0x62>
      62:	80 eb 01             	sub    $0x1,%bl
      65:	0f 84 95 00 00 00    	je     100 <ixgbe_probe+0x100>
      6b:	48 8b 45 08          	mov    0x8(%rbp),%rax
      6f:	8b 90 10 00 01 00    	mov    0x10010(%rax),%edx
      75:	80 e6 02             	and    $0x2,%dh
      78:	74 de                	je     58 <ixgbe_probe+0x58>
      7a:	bb 32 00 00 00       	mov    $0x32,%ebx
      7f:	eb 1a                	jmp    9b <ixgbe_probe+0x9b>
      81:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
      88:	bf e8 03 00 00       	mov    $0x3e8,%edi
      8d:	e8 00 00 00 00       	call   92 <ixgbe_probe+0x92>
      92:	80 eb 01             	sub    $0x1,%bl
      95:	74 59                	je     f0 <ixgbe_probe+0xf0>
      97:	48 8b 45 08          	mov    0x8(%rbp),%rax
      9b:	8b 90 00 2f 00 00    	mov    0x2f00(%rax),%edx
      a1:	83 e2 08             	and    $0x8,%edx
      a4:	74 e2                	je     88 <ixgbe_probe+0x88>
      a6:	8b 98 00 02 00 00    	mov    0x200(%rax),%ebx
      ac:	c7 80 00 02 00 00 8e 	movl   $0x8e8e8e8e,0x200(%rax)
      b3:	8e 8e 8e
      b6:	8b 80 00 02 00 00    	mov    0x200(%rax),%eax
      bc:	25 ef cf cf cf       	and    $0xcfcfcfef,%eax
      c1:	3d 8e 8e 8e 8e       	cmp    $0x8e8e8e8e,%eax
      c6:	75 44                	jne    10c <ixgbe_probe+0x10c>
      c8:	bf 0a 00 00 00       	mov    $0xa,%edi
      cd:	e8 00 00 00 00       	call   d2 <ixgbe_probe+0xd2>
      d2:	48 8b 45 08          	mov    0x8(%rbp),%rax
      d6:	89 98 00 02 00 00    	mov    %ebx,0x200(%rax)
      dc:	8b 80 00 02 00 00    	mov    0x200(%rax),%eax
      e2:	39 c3                	cmp    %eax,%ebx
      e4:	75 26                	jne    10c <ixgbe_probe+0x10c>
      e6:	48 83 c4 08          	add    $0x8,%rsp
      ea:	31 c0                	xor    %eax,%eax
      ec:	5b                   	pop    %rbx
      ed:	5d                   	pop    %rbp
      ee:	c3                   	ret
      ef:	90                   	nop
      f0:	48 83 c4 08          	add    $0x8,%rsp
      f4:	b8 92 ff ff ff       	mov    $0xffffff92,%eax # Write -110 ( ETIMEDOUT to eax)
      f9:	5b                   	pop    %rbx
      fa:	5d                   	pop    %rbp
      fb:	c3                   	ret
      fc:	0f 1f 40 00          	nopl   0x0(%rax)
     100:	48 83 c4 08          	add    $0x8,%rsp
     104:	b8 ed ff ff ff       	mov    $0xffffffed,%eax
     109:	5b                   	pop    %rbx
     10a:	5d                   	pop    %rbp
     10b:	c3                   	ret
     10c:	48 83 c4 08          	add    $0x8,%rsp
     # As can be seen below here, there's 2 returns for -EIO, when 3 branches returning -EIO.
     # This can be explained with pop instructions. First ret has pop instructions, and second is not.
     # This means 1 or 2 of our branches are returning before pushing something to stack,
     # And also 1 or 2 is returning 'after' pushing to stack.
     # If this wasn't the case, branch merging will use 1 ret for 3 branches.
     110:	b8 fb ff ff ff       	mov    $0xfffffffb,%eax
     115:	5b                   	pop    %rbx
     116:	5d                   	pop    %rbp
     117:	c3                   	ret
     118:	b8 fb ff ff ff       	mov    $0xfffffffb,%eax
     11d:	c3                   	ret
     # As can be seen, there's no ret instruction in hot path, thanks to unlikely macro.