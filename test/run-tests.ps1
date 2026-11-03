# ============================================================================
#  run-tests.ps1  -  Compila e roda a suite de testes no QEMU (sem placa).
# ----------------------------------------------------------------------------
#  Uso, a partir da raiz do repositorio:
#
#      .\test\run-tests.ps1            # compila e roda
#      .\test\run-tests.ps1 -Quiet     # so o resultado de cada caso
#
#  Codigo de saida: 0 se tudo passou, 1 se houve falha ou travamento.
#
#  Pre-requisito, uma vez por maquina:
#      python $env:IDF_PATH\tools\idf_tools.py install qemu-riscv32
# ============================================================================

[CmdletBinding()]
param(
    # Esconde o log de boot do ESP-IDF; mostra apenas os casos e o resumo.
    [switch]$Quiet,
    # Tempo maximo esperando a suite terminar.
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'

# Encerra um processo e todos os descendentes. O idf.py roda o emulador como
# processo filho: matar so o pai deixa o qemu orfao, segurando a porta 5555 e
# fazendo a execucao seguinte falhar.
function Stop-ProcessTree {
    param([int]$ProcessId)
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$ProcessId" `
                  -ErrorAction SilentlyContinue)
    foreach ($child in $children) { Stop-ProcessTree -ProcessId ([int]$child.ProcessId) }
    Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
}

# 1) Ambiente do ESP-IDF, se ainda nao estiver carregado neste terminal.
if (-not $env:IDF_PATH) {
    # *> $null e nao | Out-Null: o perfil imprime o banner com Write-Host, que
    # no PowerShell 5.1 nao passa pelo pipeline.
    . C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1 *> $null
    if (-not $env:IDF_PATH) {
        throw 'nao foi possivel carregar o ambiente do ESP-IDF (perfil do EIM)'
    }
}

# Evita o aviso "Your environment is not configured to handle Unicode
# characters" do idf.py em consoles com code page legada.
$env:PYTHONUTF8 = '1'

# 2) O QEMU nao entra no PATH pelo perfil do EIM (ele e gerado na instalacao do
#    framework, antes do QEMU existir). Descobrimos a versao instalada.
$qemuRoot = Join-Path $env:IDF_TOOLS_PATH 'tools\qemu-riscv32'
if (-not (Test-Path $qemuRoot)) {
    throw "QEMU nao instalado. Rode: python `$env:IDF_PATH\tools\idf_tools.py install qemu-riscv32"
}
$qemuVersion = (Get-ChildItem $qemuRoot -Directory | Sort-Object Name -Descending |
                Select-Object -First 1).Name
$env:PATH = (Join-Path $qemuRoot "$qemuVersion\qemu\bin") + ";$env:PATH"

# 3) Limpa emuladores de execucoes anteriores: um qemu vivo mantem a porta
#    5555 ocupada e a execucao nova nem sobe.
$stray = @(Get-Process qemu-system-riscv32 -ErrorAction SilentlyContinue)
if ($stray.Count -gt 0) {
    Write-Host "== encerrando $($stray.Count) emulador(es) de execucao anterior" -ForegroundColor Yellow
    $stray | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

# 4) Compila.
Write-Host '== compilando os testes...' -ForegroundColor Cyan
idf.py -C $PSScriptRoot build | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'FALHA ao compilar os testes' -ForegroundColor Red
    exit 1
}

# 5) Roda no QEMU acompanhando a saida ao vivo. O app imprime um marcador final;
#    ao ve-lo, encerramos o emulador (o QEMU nao sai sozinho quando o app_main
#    retorna -- a CPU emulada continua de pe).
Write-Host '== rodando no QEMU...' -ForegroundColor Cyan
$logFile = Join-Path ([System.IO.Path]::GetTempPath()) "lora-tests-$PID.log"
# python -u: sem o modo nao bufferizado, as ultimas linhas (inclusive o
# marcador final) ficariam presas no buffer de stdout ate o processo sair -- e
# como quem encerra o processo somos nos, elas nunca apareceriam.
$qemu = Start-Process -FilePath 'python' `
    -ArgumentList '-u', "$env:IDF_PATH\tools\idf.py", '-C', $PSScriptRoot, 'qemu' `
    -RedirectStandardOutput $logFile -RedirectStandardError "$logFile.err" `
    -NoNewWindow -PassThru

$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$failures = $null
$died = $false

# Leitura incremental com FileShare.ReadWrite: o arquivo esta sendo escrito por
# outro processo, e reler tudo a cada volta (Get-Content) devolve resultado
# parcial quando o arquivo esta travado.
while (-not (Test-Path $logFile)) {
    if ((Get-Date) -ge $deadline) { break }
    Start-Sleep -Milliseconds 200
}
$stream = $null
if (Test-Path $logFile) {
    $stream = [System.IO.StreamReader]::new(
        [System.IO.FileStream]::new($logFile, [System.IO.FileMode]::Open,
                                    [System.IO.FileAccess]::Read,
                                    [System.IO.FileShare]::ReadWrite))
}

# Imprime uma linha ja completa, com cor conforme o conteudo.
function Write-TestLine {
    param([string]$Text, [switch]$OnlyResults)
    if ($OnlyResults -and
        $Text -notmatch ':(PASS|FAIL)|Tests .*Failures|FIM DOS TESTES') { return }
    if ($Text -match ':FAIL')     { Write-Host $Text -ForegroundColor Red }
    elseif ($Text -match ':PASS') { Write-Host $Text -ForegroundColor DarkGray }
    elseif ($Text -match 'FIM DOS TESTES|Tests .*Failures|^OK$') { Write-Host $Text -ForegroundColor Cyan }
    else                          { Write-Host $Text }
}

# ReadLine() nao serve aqui: ao chegar no fim do arquivo antes de o escritor
# gravar o "\n", ele devolve o pedaco incompleto como se fosse uma linha, e a
# saida sai picada no meio das palavras. Lemos em blocos e so entregamos a
# linha quando o terminador chega.
$chunk = [char[]]::new(4096)
$pending = ''

while ($null -ne $stream -and (Get-Date) -lt $deadline) {
    $read = $stream.Read($chunk, 0, $chunk.Length)
    if ($read -le 0) {
        if ($qemu.HasExited) { $died = $true; break }
        Start-Sleep -Milliseconds 150
        continue
    }

    $pending += [string]::new($chunk, 0, $read)
    while (($break = $pending.IndexOf("`n")) -ge 0) {
        $line = $pending.Substring(0, $break).TrimEnd("`r")
        $pending = $pending.Substring($break + 1)

        if ($line -match 'FIM DOS TESTES: (\d+) falha') { $failures = [int]$Matches[1] }
        Write-TestLine -Text $line -OnlyResults:$Quiet
    }

    if ($null -ne $failures) { break }
}
if ($pending -match '\S') { Write-TestLine -Text $pending -OnlyResults:$Quiet }
if ($null -ne $stream) { $stream.Dispose() }

if (-not $qemu.HasExited) { Stop-ProcessTree -ProcessId $qemu.Id }

# 6) Veredito.
if ($null -eq $failures) {
    if ($died) {
        Write-Host '== o emulador encerrou antes de a suite terminar' -ForegroundColor Red
    } else {
        Write-Host "== a suite nao terminou em $TimeoutSeconds s (travou ou nao subiu)" -ForegroundColor Red
    }
    # Diagnostico: sem isto, -Quiet esconderia justamente a causa.
    foreach ($f in @($logFile, "$logFile.err")) {
        $tail = @(Get-Content $f -Tail 15 -ErrorAction SilentlyContinue |
                  Where-Object { $_ -match '\S' })
        if ($tail.Count -gt 0) {
            Write-Host "-- ultimas linhas de $(Split-Path $f -Leaf):" -ForegroundColor Yellow
            $tail | ForEach-Object { Write-Host "   $_" }
        }
    }
    Write-Host "-- log completo em: $logFile" -ForegroundColor Yellow
    exit 1
}

Remove-Item $logFile, "$logFile.err" -ErrorAction SilentlyContinue
if ($failures -gt 0) {
    Write-Host "== $failures teste(s) falharam" -ForegroundColor Red
    exit 1
}
Write-Host '== todos os testes passaram' -ForegroundColor Green
exit 0
