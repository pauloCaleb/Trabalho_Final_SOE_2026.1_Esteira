/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Esteira Separadora - GUI Raspberry Pi", "index.html", [
    [ "Esteira Separadora — Guia de Utilização", "md__r_e_a_d_m_e.html", [
      [ "Requisitos de Hardware", "md__r_e_a_d_m_e.html#autotoc_md2", null ],
      [ "Configuração da UART no Raspberry Pi", "md__r_e_a_d_m_e.html#autotoc_md4", null ],
      [ "Dependências", "md__r_e_a_d_m_e.html#autotoc_md6", null ],
      [ "Compilação", "md__r_e_a_d_m_e.html#autotoc_md8", null ],
      [ "Uso", "md__r_e_a_d_m_e.html#autotoc_md10", [
        [ "Exemplos", "md__r_e_a_d_m_e.html#autotoc_md11", null ]
      ] ],
      [ "Modos de Operação", "md__r_e_a_d_m_e.html#autotoc_md13", [
        [ "Modo FSM (1) — Operação Autônoma", "md__r_e_a_d_m_e.html#autotoc_md14", null ],
        [ "Modo DEBUG (2) — Controle Manual", "md__r_e_a_d_m_e.html#autotoc_md15", null ],
        [ "Modo HMI (3) — Display LCD + Botões START/STOP + Buzzer", "md__r_e_a_d_m_e.html#autotoc_md16", null ]
      ] ],
      [ "Formato do QR Code", "md__r_e_a_d_m_e.html#autotoc_md18", null ],
      [ "Saída no Terminal", "md__r_e_a_d_m_e.html#autotoc_md20", null ]
    ] ],
    [ "Esteira Separadora — Descrição Técnica dos Algoritmos", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html", [
      [ "Visão Geral da Arquitetura", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md23", null ],
      [ "Protocolo de Comunicação (proprietário, binário, UART 8N1)", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md25", [
        [ "Características", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md26", null ],
        [ "Estrutura dos Frames", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md27", null ],
        [ "Tabela de Comandos", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md28", null ]
      ] ],
      [ "Módulos", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md30", [
        [ "<span class=\"tt\">serial.c</span> — Camada Física", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md31", null ],
        [ "<span class=\"tt\">protocol.c</span> — Parser de Frames", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md33", null ],
        [ "<span class=\"tt\">fsm.c</span> — Máquina de Estados (Modo Autônomo)", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md35", [
          [ "Diagrama de Estados", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md36", null ],
          [ "Arquitetura de Threads", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md37", null ]
        ] ],
        [ "<span class=\"tt\">camera.cpp</span> — Captura e Leitura de QR", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md39", [
          [ "Fluxo de captura", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md40", null ],
          [ "Formato esperado do QR", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md41", null ]
        ] ],
        [ "<span class=\"tt\">debug.c</span> — Modo Interativo", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md43", null ],
        [ "<span class=\"tt\">hmi.c</span> — Modo Display + Botões + Buzzer", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md45", [
          [ "Alerta de QR não identificado e recuperação do handshake", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md46", null ],
          [ "Contadores de objetos entregues", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md47", null ],
          [ "Dependências de hardware, isoladas em módulos próprios", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md48", null ]
        ] ],
        [ "<span class=\"tt\">log.c</span> — Sistema de Log", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md50", null ]
      ] ],
      [ "Sequência de Inicialização (Handshake)", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md52", null ],
      [ "Dependências e Versões", "md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md54", null ]
    ] ],
    [ "Data Structures", "annotated.html", [
      [ "Data Structures", "annotated.html", "annotated_dup" ],
      [ "Data Structure Index", "classes.html", null ],
      [ "Data Fields", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"md__r_e_a_d_m_e___t_e_c_n_i_c_o.html#autotoc_md41"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';