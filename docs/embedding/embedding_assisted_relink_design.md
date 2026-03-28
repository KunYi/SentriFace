# Embedding-Assisted Relink Design

## 1. 目的

這份文件定義一條輕量的 `embedding-assisted re-link` 策略。

它要解的不是一般多目標追蹤問題，而是門禁場景中更常見的狀況：

- 同一個人短時間內轉頭或移動
- tracker 產生新的 `track_id`
- 但系統其實只需要把這些短斷軌視為同一次通行事件

---

## 2. 為什麼不直接做重型外觀追蹤

理論上可以在 tracking association 階段直接加入外觀特徵。

但對目前專案來說，這通常不是第一優先：

- `RV1106` 算力有限
- 門禁不是長時間多人交錯監控
- 我們不會對每一幀都做 embedding

因此更合理的做法是：

- 平常仍以 `IoU + KF` 為主
- 對已經算過的 embedding 做事件層輔助 re-link

這也符合產品化前提：

- 不追求每幀最優 identity
- 優先追求 `2 秒內` 能得到穩定門禁結果

---

## 3. 適用前提

這條規則最適合：

- 同一個人
- 時間很短，通常 `1 ~ 2` 秒內
- 空間位置接近
- 已經有至少一次可用 embedding

也就是：

- 不要求每幀都有 embedding
- 只利用已存在的 embedding cache
- 只要在短時間窗內能支持最終 decision 即可

---

## 4. 核心規則

當新 track 出現時，可嘗試和舊 track 做 re-link，但需同時滿足：

1. 時間接近
   - `gap_ms <= short_gap_merge_window_ms`

2. 空間接近
   - 中心點位移不可過大

3. 尺寸合理
   - bbox 面積比例不可過度失真

4. 身份或特徵成立其一
   - `stable_person_id` 一致
   - 或 embedding cosine similarity 達門檻

目前設計方向是：

- `same_person_id OR strong_embedding_similarity`

而不是要求兩者都成立。

另外第一版產品化應維持一條保守規則：

- 前一段 track 本身必須已經有基本穩定度

也就是：

- 不是任何短時間斷軌都能只靠 embedding 接回
- 若前一段 track 連最基本的 consistent hits 都不夠，應避免過早合併

---

## 5. 為什麼這樣設計

這樣做有幾個好處：

- 若 search / DB 結果已經穩定，re-link 很容易成立
- 若 search 暫時還不穩，但 embedding 很像，仍可接回同一次 session
- 不需要把外觀特徵硬塞進每幀 tracker association

這很符合門禁使用方式：

- 系統更在意最後 unlock 是否穩定
- 不需要追求每一幀的全局最優 tracking identity
- 也不需要強求所有短斷軌都重接成單一 `track_id`

所以更精確地說：

- embedding 是 relink 的輔助訊號
- 不是用來替代「原本就要夠穩」這件事

---

## 6. 目前介面落點

目前這條能力已放在：

- `include/app/face_pipeline.hpp`
- `src/app/face_pipeline.cpp`

也就是：

- tracker 保持幾何型追蹤主線
- pipeline / decision 層負責事件級 re-link

---

## 7. 目前限制

目前這條能力雖然已經有介面與測試骨架，但要真正對實拍素材發揮效果，仍需要：

- 真實 embedding event 流
- track 級 embedding cache
- 對應的 offline / replay 驗證素材
- 合理的 embedding 觸發節流策略

因此現階段比較正確的定位是：

- 能力與接口已建立
- 真實效果仍待後續接入 embedding data 驗證

---

## 8. 對本專案的意義

這條設計很重要，因為它剛好補上兩件事之間的空隙：

- `IoU + KF` 幾何追蹤雖然輕量，但偶爾會切段
- embedding 雖然不會每幀都做，但一旦已有結果，就可以用來補事件連續性

所以它很適合作為目前門禁系統的中間解法：

- 比重型 appearance tracker 輕
- 比純 `track_id` 綁定穩
- 比「每幀都跑 embedding」更符合 `RV1106` 產品化限制
