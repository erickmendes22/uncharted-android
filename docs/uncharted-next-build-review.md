# Uncharted Android — revisão de controles e estabilidade temporal
**Status:** revisão de código; não liberar APK antes da aprovação explícita.  
**Base:** ActionIconsV3, commit `b63ea45f6de0faa656e15331cb54b7e75e4689fc`.  
**Referência:** capturas reais de Uncharted 2 e a referência visual GoW 3 fornecidas pelo usuário.

## 1. Layout de gameplay (coordenadas lógicas 1280 × 720)

As posições usam a **largura completa da tela** em formato panorâmico, preservando círculos redondos e áreas de toque. Tamanho-padrão do estilo Uncharted: 100%; raio dos botões de ação: 46 unidades; analógicos: 80 unidades. Os valores são os centros `(x,y)`. A ordem reproduz `TouchControllerOverlay.NAMES`:

| Controle | Centro | Ação, visual e critério |
|:--|:--|:--|
| L1 | (112,108) | Granada, ícone dourado de granada |
| L2 | (260,108) | Mirar, retículo |
| TOUCH | (585,65) | Touchpad/menus, dois retângulos, original |
| OPTIONS | (690,65) | Pausa/menu, original |
| R1 | (1016,108) | Recarregar, munição em seta |
| R2 | (1166,108) | **Atirar: nova pistola legível, apontada à direita, com pequeno disparo na boca do cano** |
| D-pad | (145,302) | Secundário; menor e separado do analógico |
| △ | (1125,285) | **Interagir: nova mão aberta, palma e dedos separados** |
| □ | (980,402) | Soco/corpo a corpo, punho |
| ○ | (1190,455) | Cobertura/agachar atrás de obstáculo |
| × | (1050,565) | Saltar/escalar, figura em movimento |
| Analógico esquerdo | (142,570) | Movimento; círculo e ponto móvel |
| Analógico direito | (825,575) | **Câmera**; círculo e ponto móvel, sem ícone fotográfico |
| L3 / R3 | próprios analógicos | Dois toques rápidos no analógico correspondente; sem bolhas extras e sem L3+R3 combinado |

O lado direito **não é um diamante de controle PlayStation**. Interação está acima; ataque corpo a corpo no lado interno, cobertura na borda direita e salto na região inferior, deixando a área do polegar no analógico direito disponível. Os textos L1/L2/R1/R2 e △/□/○/× ficam centralizados **abaixo dos círculos**, e dentro ficam **apenas os pictogramas das ações**. Os controles esquerdo e direito obedecem ao mesmo `PadBridge` e mantêm o suporte ao gamepad físico.

## 2. Opção de layouts e editor

- **Uncharted (ícones de ação)** e **Clássico PlayStation** continuam alternáveis pelo launcher e pelo menu in-game.
- Cada estilo preserva separadamente posições, escala e opacidade; o estilo Clássico não deve ser modificado nesta revisão.
- `ACTION_LAYOUT_VERSION=4` migra apenas o layout Uncharted, evitando que posições antigas contaminem a atualização.
- Arrastar, redimensionar e restaurar os botões deve usar as coordenadas exibidas **e** as mesmas áreas reais de toque em telas 16:9 e panorâmicas.
- Não introduzir botão de câmera extra nem atalhos combinados L3+R3.

## 3. FSR 3.1 / Snapdragon GSR 2: mudanças candidatas identificadas no código

### 3.1 Formato LDR das entradas: correção baseada em diferença confirmada

O fork `RedVorteX-TV/Uncharted-arm` v0.1.8 usa `LdrFormat()`: reinterpreta `R8G8B8A8Srgb`/ `B8G8R8A8Srgb` como UNORM ao criar a *view* de entrada do upscale. O nosso `RunScaled()` usava diretamente `color.info.pixel_format` em `EnsureUiResources`, `CachedView` e `Describe`. A diferença pode provocar descodificação gama indesejada e diferentes formatos de imagem nos caminhos temporais. Não é demonstração suficiente de causa das piscadas.

A revisão acrescenta `UnchartedLdrViewFormat()` **apenas para FSR 3.1 e GSR 2**, mantendo o TAA, que aparece funcional na captura, sem alteração. A alteração pode ser desabilitada para A/B com `BB_UNCHARTED_SRGB_VIEW=0`.

### 3.2 Evitar quadro host vazio/antigo em falhas transitórias

Em `BeforeDisplay()`, quando a cena atual chegou ao ponto de composição mas `camera_motion.Ready()` é falso, ou quando o dispatch não definiu `ui_phase`, a revisão usa `RunUiOnly(source,camera_motion.Depth())`. É um fallback espacial usando o **quadro guest atual**, em vez de insistir no histórico de um quadro anterior. Na volta ao temporal, `reset=true`. Essa escolha protege contra alguns quadros pretos; não prova nem elimina todos os tipos de flicker de movimento.

O modo TAA e o bypass dos vídeos/cutscenes do U2 permanecem inalterados. Fallback é marcado nos primeiros 12 registros com `Upscaler: Uncharted fallback current-frame ...`.

### 3.3 Itens ainda a validar antes de chamar o bug de resolvido

1. **Jitter e vetores de movimento:** comparar sinal, escala, deslocamento de câmera e histórico somente após confirmar se a falha aparece em `input`, `motion` ou `output`.
2. **Barreiras e tempo de vida:** verificar `ui_image` e `output_image`, aquisição de textura e exibição do *display buffer* nos quadros com piscada.
3. **Histórico temporal:** validar `reset` em transições 3D/vídeo, alteração de resolução e troca de provedor; não resetar em todo quadro.
4. **GSR 2:** inspecionar separadamente `gsr2_convert.comp` e `gsr2_upscale.comp`; não ativar mudanças de filtro como tentativa de corrigir quadros pretos.
5. **FSR 3.1:** preservar os patches de antialiasing já validados para o `eboot`, e conferir retorno de `ffxVkPortableUpscaleContextRecordDispatch` por quadro problemático.

## 4. Plano de validação — antes de liberar APK

- **Revisão estática:** os testes devem conferir geometria/ausência de sobreposição em quatro proporções, opção Clássico, dois analógicos, R2/△ vetoriais, conversão sRGB/UNORM, fallback corrente e bypass de vídeos.
- **Compilação após aprovação:** pacote versionado e instalável sobre a versão anterior, sem alterar assinatura nem nome interno do app.
- **A/B no aparelho com a mesma cena e câmera:** TAA (controle), FSR 3.1 Performance e GSR 2; capturar os trechos de tela com piscadas, qualidade de UI e taxas de quadros.
- **Se piscadas persistirem:** comparar FSR/GSR com `BB_UNCHARTED_SRGB_VIEW=0`, coletar registros `Upscaler: Uncharted fallback...` e examinar os buffers nos quadros problemáticos. Não confundir FPS reportado pelo HUD com taxa de quadros corretos.
- **Aprovação final:** imagem contínua em gameplay, sem flashes pretos na transição com vídeos, controles alcançáveis, gestos da câmera e físico funcionando, sem regressão perceptível no TAA.

**O fato de os testes estáticos passarem ou a build compilar não equivale a confirmar estabilidade no telefone.**
