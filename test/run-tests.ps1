# ============================================================================
#  run-tests.ps1  -  Compila e roda a suite de testes no QEMU (sem placa).
# ----------------------------------------------------------------------------
#  Uso, a partir da raiz do repositorio:
#
#      .\test\run-tests.ps1
#
#  Pre-requisito, uma vez por maquina:
#      python $env:IDF_PATH\tools\idf_tools.py install qemu-riscv32
# ============================================================================

$ErrorActionPreference = 'Stop'

# 1) Ambiente do ESP-IDF, se ainda nao estiver carregado neste terminal.
if (-not $env:IDF_PATH) {
    . C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
}

# 2) O QEMU nao entra no PATH pelo perfil do EIM (ele e gerado na instalacao do
#    framework, antes do QEMU existir). Descobrimos a versao instalada.
$qemuRoot = Join-Path $env:IDF_TOOLS_PATH 'tools\qemu-riscv32'
if (-not (Test-Path $qemuRoot)) {
    throw "QEMU nao instalado. Rode: python `$env:IDF_PATH\tools\idf_tools.py install qemu-riscv32"
}
$qemuVersion = (Get-ChildItem $qemuRoot -Directory | Sort-Object Name -Descending |
                Select-Object -First 1).Name
$env:PATH = (Join-Path $qemuRoot "$qemuVersion\qemu\bin") + ";$env:PATH"

# 3) Compila e roda. O app de teste imprime o resultado de cada caso e termina.
$testDir = Join-Path $PSScriptRoot ''
idf.py -C $testDir build
if ($LASTEXITCODE -ne 0) { throw "falha ao compilar os testes" }

idf.py -C $testDir qemu
