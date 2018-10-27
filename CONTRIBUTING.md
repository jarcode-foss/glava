
## Code Style

GLava uses a bastardized version of the [linux kernel style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html), with the following modifications:

* Opening braces are _always_ on the same line as the token it is associated with (`if`, `while`, labels, functions). The only time this is not honoured is when a set of braces has no associated token (ie. scope usage).

* Indentation is 4 spaces, and tabs are forbidden

* The content of a `switch` statement, including `case` labels, are indented.

* Preprocessor directives should inherit the same intentation level as the code it resides in.

* Align tokens in repetitious lines by padding spacing between tokens.

The following rules of the linux style are **ignored**:

* Function size and control flow recommendations
* Comment formatting rules
* Any other rules regarding preprocessor directives

Naming rules and the usage of `typedef` is strictly honoured from the Linux style. Anything not mentioned here is probably subjective and won't hurt your chances of getting a PR accepted.

If you use GNU Emacs, the above style can be configured via the following elisp:

```emacs
(setq-default c-basic-offset 4)
(setq c-default-style "linux")
(setq tab-stop-list (number-sequence 4 200 4))
(c-set-offset (quote cpp-macro) 0 nil)
(c-set-offset 'case-label '+)
```

## Shaders

If you author and maintain your own shader module for GLava, you are free to use your preferred code style. Otherwise, shaders follow the same style as GLava's C sources.

## Pull Requests

You are free to make pull requests for any change, even if you are not sure if the proposed changes are appropriate. @wacossusca34 and/or @coderobe will be able to suggest changes or commentary on the PR if there is a reason it is not acceptable.

## Conduct

Engagement in the issue tracker and pull requests simply requires participants remain rational and on-topic.