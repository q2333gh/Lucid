# LLM_C Step Test (GGUF)

Goal: always run step inference on the IC canister using GGUF.

Default GGUF path: `lib/tiny-gpt2-f16.gguf` (uploaded as asset `model`).

## Run (PocketIC)

```bash
python tests/test_llm_c.py --use-pocketic --with-stories --stories-length 16 --max-new 1
```

What it does:
- Uploads `model` (GGUF), `vocab`, `merges`, and a small `stories` slice.
- Calls `infer_stories_step_init`, `infer_stories_step`, then `infer_stories_step_get`.

## Notes

- `--stories-length` must fit the model context (tiny-gpt2 uses small context).
- Increase `--max-new` to generate more tokens per step.
