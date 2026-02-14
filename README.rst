Userspace NIC driver experiment for low latency workloads
==============================================
This repository contains a proof-of-concept ixgbe driver for the architecture detailed in
`Architectural design principles`_.
Although the architecture is designed for mlx5 driver, proof of concept driver is leveraging 82599.

Some of the reasons are:

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
-------------------------------

Radical Cropping
^^^^^^^^^^^^^^^^^

This title includes keeping everything that doesn't help us achieve the goal out. For example, in this proof of concept using the ICMP protocol, checksums are not validated upon ingress and will not be recalculated when replying. Instead, RFC 1624 is used.
Destination IP is not going to be checked. Instead, the logic will just switch destination and source IP's. As can be seen in these points, a new packet will not be generated. Instead, received packet will be edited and bumped to wire as soon as possible.

Inlining Workload with Driver
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This section holds a critical place among the ideas of this project. Even if it is hard to maintain, portability is impossible on NIC-specific logic, (e.g, Initializing 82599 part in the TO-DO) if it's profitable, it can be done.
If this repository succeeds and continued, this question will be answered in an article. In the continued implementation, Mellanox ASICs will be used, and features like WQE inlining are leveraged.
HFT vision of Mellanox dates back to 2012, perhaps even earlier. See https://network.nvidia.com/files/pdf/whitepapers/SB_HighFreq_Trading.pdf for details.

TO-DO
-----

* |checked| **Setting Environment**
    -  |checked| Unbind the device from kernel's control permanently.
By permanently, preventing kernel from taking control undesirably is meant.
    -  |checked| Allocate a hugepage & map with 'Memory based I/O'.
With 2 MB hugepage usage, TLB misses are reduced with the cost of 2 MB memory.
**Tested on bare metal.**

* |ballot| **Initializing 82599**
    -  |checked| Architectural designs for performance.
Some of these designs are documented in this file, or at the /docs directory.
    -  |ballot| Implement register manipulations following the design made for initializing.
Work in progress. With Pull Requests section, it can be observed.
    -  |ballot| Tx/Rx ring management.

* |ballot| **Rewriting ICMP from scratch to benchmark**
    -  |ballot| Implement logic based on design made.
    -  |ballot| Collecting metrics and analysis.
After that analysis, implementing future logic will be decided.

[1] Mellanox Firmware Design Architecture, see Programmer's Reference Manual. Could not refer to a topic or page because it's mentioned in many. Note that PRM's are not public, but ConnectX-4.

.. |ballot| unicode:: U+2610 .. empty box
.. |checked| unicode:: U+2611 .. box with check
