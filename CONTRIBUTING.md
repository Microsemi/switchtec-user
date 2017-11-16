# Contributing

Contributions are most welcome but it would be very much appreciated if
you follow the following guidelines.

## Style

It's simply polite to match the style of the code you are contributing to.
Code that does not follow the style will not be looked upon kindly. This
code largely follows the [Linux Kernel Style][1] so please read that
document and take heed of its wisdom.

## Pull Requests

This project uses Travis to check that it always builds on all the supported
platforms. However, Microsemi's IT department stubbornly refuses (for silly
reasons) to turn it on for the main repository. Therefore, in order to ensure
your contributions get tested, please submit all pull requests to
[lsgunth/switchtec-user][2]. From there we will ensure your contribution
gets merged into the main repository.

## Pitfalls

* In the code, ensure to avoid accessing the GAS directly except in an
  appropriate platform file or a new command that calls switchtec_gas_map().
  Also, be aware that on Linux, using the GAS directly requires full root
  privilages where as all other commands only require access to the device file.


[1]: https://www.kernel.org/doc/html/latest/process/coding-style.html
[2]: https://github.com/lsgunth/switchtec-user/