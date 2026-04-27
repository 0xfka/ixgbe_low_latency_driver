Application (IEX Exchange) logic inlined userspace NIC driver from scratch
==========================================================================
This repository contains a userspace inlined NIC driver experiment, with the architecture detailed in `Architectural design principles`_. 
Although the architecture is designed for mlx5 driver, proof of concept driver is leveraging 82599.

Installation
============
Prerequisites
^^^^^^^^^^^^^
*    **2 x 2 MB hugepage**
add via command:

.. code-block:: console

    echo x > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

Since 2 hugepage is enough for this driver, already used hugepage count should be checked via Cat command.
In NUMA, the path may change.

-    82599 Chipset NIC
-    Cmake 3.10 or higher
-    zlib1g for hdr histogram
-    Any C compiler that supports C99, which was released on 1999.
Installation commands can be found on Github Matrix Build workflow.

Build from source 
^^^^^^^^^^^^^^^^^
.. code-block:: console

    git clone https://github.com/0xfka/ixgbe-userspace-poc

    cd ixgbe-userspace-poc
    # For debug mode: 
    cmake -B build -DDEBUG_MODE=ON && cmake --build build 
    # OFF: 
    cmake -B build -DDEBUG_MODE=OFF && cmake --build build 
    # As stated in CmakeLists.txt, "# At past, I was used printf in some parts to prove the logic works as expected".
    # Debug mode is used to enable/disable that.

Building on popular distros are tested via Github Actions. If you can see a green tick on the repository, chances on you probably doesn't encounter any errors.

Why 82599:
==========
Direct Register Manipulation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Instead of sending commands to a "mailbox[1]", direct register manipulation is considered more transparent when it comes to understanding & optimizing at a low level.
As a trade-off, while direct register manipulation may have slightly higher latency than mailbox commands, 82599 is preferred because modern NICs possess multilayered abstractions that obscure low-level behavior.

Defined hardware behavior & Transparent documentation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
82599 Datasheet is crystal clear, and potential edge cases are documented over the years. As mentioned in previous section, even initializing an 82599 ASIC vibes like playing with electrons manually.
Similar attempt on a modern NIC would feel like sending mails to 'mailbox', since it is.

Simpler Internal Logic
^^^^^^^^^^^^^^^^^^^^^^
As an ASIC released in July 2011, of course it's simpler than modern NIC ASICs. When it comes to architecture, lean & mean is preferred. Except for a few features(e.g, WQE inlining on Mellanox), many of the offered features are bloat for low latency goal. With these points are kept in mind, 82599 is selected for proof of concept, and modern Mellanox NICs (Model may vary) is selected for going production, if the architecture designed works well enough on 82599.
This work aims to prove the architecture and become aware of situations not taken into consideration.

Architectural design principles
============================== 

Path Stripping
^^^^^^^^^^^^^^

This title includes keeping everything that doesn't help us achieve the goal out. For example, in this proof of concept using the ICMP protocol, checksums are not validated upon ingress and will not be recalculated when replying. Instead, RFC 1624 is used.
Destination IP is not going to be checked. Instead, the logic will just switch destination and source IP's. As can be seen in these points, a new packet will not be generated. Instead, received packet will be edited and bumped to wire as soon as possible.
RFC 1624 design is tested on IMCP release, and also the increasement was hardcoded, which even lowers the cost.

Inlining Workload with Driver
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This section holds a critical place among the ideas of this project. Even if it is hard to maintain, portability is impossible on NIC-specific logic, (e.g, Initializing 82599 part in the TO-DO) if it's profitable, it can be done.
If this repository succeeds and continued, this question will be answered in an article. In the continued implementation, Mellanox ASICs will be used, and features like WQE inlining are leveraged.
HFT vision of Mellanox dates back to 2012, perhaps even earlier. See https://network.nvidia.com/files/pdf/whitepapers/SB_HighFreq_Trading.pdf for details.
This design allows us to prevent swapping between user - kernel spaces, which can be proven via perf:      

.. code-block:: console

    5.421540387 seconds time elapsed

    5.416721000 seconds user
    0.002252000 seconds sys

TO-DO
-----
* |checked| **Release 0.1 - Ping reply**
* |checked| **Setting Environment**
    -  |checked| Unbind the device from kernel's control permanently.
By permanently, preventing kernel from taking control undesirably is meant.
    -  |checked| Allocate a hugepage & map with 'Memory based I/O'.
With 2 MB hugepage usage, TLB misses are reduced with the cost of 2 MB memory.
**Tested on bare metal.**

* |checked| **Initializing 82599**
    -  |checked| Architectural designs for performance.
Some of these designs are documented in this file, or at the /docs directory.
    -  |checked| Implement register manipulations following the design made for initializing.
    -  |checked| Tx/Rx ring management.

* |checked| **Rewriting basic ICMP reply logic from scratch to benchmark**
    -  |checked| Implement logic based on design made.
    -  |checked| Collecting metrics and analysis.

* |checked| **Release 1.0 - IEX Exchange**
    - |checked| Fail-fast on flow control & flow integrity, which provided via sequence numbers & session ID's.
    - |ballot| Designing a B plan for losing flow integrity, such as refilling.

Contributing
============
Github Pull requests and/or issues section(s) may be used for contributing/feature request/questions or more.
Environment setup script handles pre-commit and blame for now, and may more in future. 

.. code-block:: console

    # Yes, that's all. All the configurations made is project-wide and will not affect your configurations.
    ./setup-env.sh

    
[1] Mellanox Firmware Design Architecture, see Programmer's Reference Manual. Could not refer to a topic or page because it's mentioned in many. Note that PRM's are not public, but ConnectX-4.

.. |ballot| unicode:: U+2610 .. empty box
.. |checked| unicode:: U+2611 .. box with check