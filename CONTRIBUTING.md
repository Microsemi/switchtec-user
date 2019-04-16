# Contributing

Contributions are most welcome but it would be very much appreciated if
you follow the following guidelines.

## Style

It's simply polite to match the style of the code you are contributing to.
Code that does not follow the style will not be looked upon kindly. This
code largely follows the [Linux Kernel Style][1] so please read that
document and take heed of its wisdom.

## Pull Requests

Please create the pull request basing the branch devel on this repo.
Make sure the comparing changes include your bug fixes or new features.
We follow kernel style as documented [here][1]. It is highly recommended
that the patches be checked with the checkpatch.pl script in the Linux
Kernel tree with no obivous style problems. Once the patches go through
the review, they will be merged into the branch devel.

## Pitfalls

* In the code, ensure to avoid accessing the GAS directly except in an
  appropriate platform file or a new command that calls switchtec_gas_map().
  Also, be aware that on Linux, using the GAS directly requires full root
  privilages where as all other commands only require access to the device file.


[1]: https://www.kernel.org/doc/html/latest/process/coding-style.html
