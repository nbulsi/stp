Introduction
============

Semi-Tensor Product (STP) engine for Electronic Design Automation (EDA)

Compilation requirements
-------------------------------
Support of C++ 17 standard is required to compile stp. Now it works using GCC 8.
More comiplers are going to be tested.

Using stp as a stand-alone tool
-------------------------------

Clone ``stp`` project and compile it::

 git clone https://gitee.com/zfchu/stp.git   (Gitee repository)
 git clone https://github.com/nbulsi/stp.git (GitHub repository) 
 cd stp
 mkdir build
 cd build
 cmake ..
 make
 
You can test all cases and examples::
 
 ./test/run_tests
 ./example/matrix

The test and examples directories are compiled in defalut, if you want turn it off, please use::

 cmake -DSTP_EXAMPLES=OFF -DSTP_TEST=OFF ..

Using ``stp`` as a library in another project
------------------------------------------------

Being header-only, ``stp`` can be easily integrated into existing and new projects.
Just add the include directory of ``stp`` to your include directories, and simply
include stp by

.. code-block:: c++

   #include <stp/stp.hpp>
