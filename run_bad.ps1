param(
    [string]$File = "examples/01-basics/quick_start_demo.bad",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$PassThruArgs
)

$bin = if (Test-Path ".\bad.exe") {
    ".\bad.exe"
} elseif (Get-Command bad -ErrorAction SilentlyContinue) {
    "bad"
} else {
    throw "Could not find bad.exe or bad on PATH. Build first with 'make'."
}

& $bin $File "--config" "examples/.badrc" @PassThruArgs
exit $LASTEXITCODE
