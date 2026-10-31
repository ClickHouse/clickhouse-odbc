# AI Policy

This repository follows the [ClickHouse AI Policy](https://github.com/ClickHouse/ClickHouse/blob/master/AI_POLICY.md). The text below adapts it to the ClickHouse ODBC driver. Where the two differ, the upstream policy is canonical.

## You can use AI

You can use AI for ClickHouse ODBC driver development. We welcome and embrace AI usage, as well as research and experiments with frontier AI models and novel methods of applying AI to software engineering.

You don't have to disclose your usage of AI. You can tell us about it, share your experience, and show your methods, but it is not required. AI is a normal developer's tool, similar to an IDE, an OS, or a keyboard. We don't judge your work on the basis of AI usage, but we recommend taking efforts to filter out slop before sending a pull request; otherwise, it may negatively affect your reputation as an engineer.

## You are responsible for what you submit

When sending generated code, you take responsibility for it in the same way as for code you have typed by hand. Read and review the code before sending it; otherwise it is disrespectful to maintainers. Take efforts to understand the code base, with or without the help of AI. Low-effort pull requests that require high effort from maintainers will be closed. Do not use AI to automate your responses to maintainers.

In practice, for this repository, that means:

- Build the driver and run the tests locally before opening a pull request. See [CONTRIBUTING.md](CONTRIBUTING.md) and the [build instructions](README.md#building-from-sources).
- Check that the change behaves correctly on the platforms it touches. The driver ships for Windows, macOS, and Linux, and ODBC driver managers differ between them. CI runs on all three.
- Verify claims the model makes about the ODBC specification, driver manager behaviour, or ClickHouse server behaviour against the actual documentation or a running system. Generated explanations of ODBC semantics are frequently wrong in subtle ways.
- Do not submit a pull request whose description was written by AI unless you have checked that it accurately describes what the change does and why.

## What AI is good for here

Prefer using AI for improving the code base: removing and simplifying code, improving build speed, improving continuous integration and quality checks, adding and improving tests, reverting bad modifications, security research, and bug fixing. Keep in mind that using AI to implement large features requires as much design consideration as without AI. For significant changes to the driver's behaviour, open an issue to discuss the design first.

## Intellectual property

When using AI, the same rules around intellectual property apply as with manually written code. Do not copy, reproduce, or include code belonging to others unless its license explicitly permits this use and all license requirements are met. You are responsible for ensuring that you have all required permissions for any submitted code, whether AI-generated or not. This repository is licensed under the terms in [LICENSE](LICENSE); contributions must be compatible with it.

## Research and experiments

We are happy to participate in research and experiments with AI models and their application methods on top of the ClickHouse code bases, including this driver. This could be benchmarks and comparisons of models, testing of models by solving identical tasks, AI reproducibility studies, performance of agentic loops, AI sandboxing, and so on. If you want to share your research, you can send a letter to ai@clickhouse.com.
